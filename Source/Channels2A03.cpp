/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** 0CC-FamiTracker is (C) 2014-2015 HertzDevil
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/

// This file handles playing of 2A03 channels

#include <cmath>
#include "stdafx.h"
#include "FamiTracker.h"
#include "FamiTrackerDoc.h"
#include "ChannelHandler.h"
#include "Channels2A03.h"
#include "Settings.h"
#include "SoundGen.h"

//#define NOISE_PITCH_SCALE

CChannelHandler2A03::CChannelHandler2A03() : 
	CChannelHandler(0x7FF, 0x0F),
	m_cSweep(0),
	m_bSweeping(0),
	m_iSweep(0)
{
}

void CChannelHandler2A03::HandleNoteData(stChanNote *pNoteData, int EffColumns)
{
	m_iSweep = 0;
	m_bSweeping = false;
	// // //
	CChannelHandler::HandleNoteData(pNoteData, EffColumns);

	if (pNoteData->Note != NONE && pNoteData->Note != HALT && pNoteData->Note != RELEASE) {
		if (!m_bEnvelopeLoop || m_bHardwareEnvelope)		// // //
			m_bResetEnvelope = true;
	}
}

void CChannelHandler2A03::HandleCustomEffects(int EffNum, int EffParam)
{
	#define GET_SLIDE_SPEED(x) (((x & 0xF0) >> 3) + 1)

	if (!CheckCommonEffects(EffNum, EffParam)) {
		// Custom effects
		switch (EffNum) {
			case EF_VOLUME:
				if (EffParam < 0x20) {		// // //
					m_iLengthCounter = EffParam;
					m_bEnvelopeLoop = false;
					m_bResetEnvelope = true;
				}
				else if (EffParam >= 0xE0 && EffParam < 0xE4) {
					if (!m_bEnvelopeLoop || !m_bHardwareEnvelope)
						m_bResetEnvelope = true;
					m_bHardwareEnvelope = ((EffParam & 0x01) == 0x01);
					m_bEnvelopeLoop = ((EffParam & 0x02) != 0x02);
				}
				break;
			case EF_SWEEPUP:
				m_iSweep = 0x88 | (EffParam & 0x77);
				m_iLastPeriod = 0xFFFF;
				m_bSweeping = true;
				break;
			case EF_SWEEPDOWN:
				m_iSweep = 0x80 | (EffParam & 0x77);
				m_iLastPeriod = 0xFFFF;
				m_bSweeping = true;
				break;
			case EF_DUTY_CYCLE:
				m_iDefaultDuty = m_iDutyPeriod = EffParam;
				break;
			// // //
		}
	}
}

void CChannelHandler2A03::HandleEmptyNote()
{
	// // //
	if (m_bSweeping)
		m_cSweep = m_iSweep;
}

void CChannelHandler2A03::HandleCut()
{
	CutNote();
}

void CChannelHandler2A03::HandleRelease()
{
	if (!m_bRelease)
		ReleaseNote();
/*
	if (!m_bSweeping && (m_cSweep != 0 || m_iSweep != 0)) {
		m_iSweep = 0;
		m_cSweep = 0;
		m_iLastPeriod = 0xFFFF;
	}
	else if (m_bSweeping) {
		m_cSweep = m_iSweep;
		m_iLastPeriod = 0xFFFF;
	}
	*/
}

void CChannelHandler2A03::HandleNote(int Note, int Octave)
{
	m_iNote			= RunNote(Octave, Note);
	m_iDutyPeriod	= m_iDefaultDuty;
	m_iInstVolume	= 0x0F;		// // //

	m_iArpState = 0;

	if (!m_bSweeping && (m_cSweep != 0 || m_iSweep != 0)) {
		m_iSweep = 0;
		m_cSweep = 0;
		m_iLastPeriod = 0xFFFF;
	}
	else if (m_bSweeping) {
		m_cSweep = m_iSweep;
		m_iLastPeriod = 0xFFFF;
	}
}

bool CChannelHandler2A03::CreateInstHandler(inst_type_t Type)
{
	switch (Type) {
	case INST_2A03: case INST_VRC6: case INST_S5B:
		CREATE_INST_HANDLER(CSeqInstHandler, 0x0F, Type == INST_S5B ? 0x40 : 0); return true;
	case INST_N163:
		CREATE_INST_HANDLER(CSeqInstHandlerN163, 0x0F, 0); return true;
		/*
	case INST_FDS:
		CREATE_INST_HANDLER(CSeqInstHandlerFDS, 0x1F, 0); return true;
	case INST_VRC7:
		CREATE_INST_HANDLER(CInstHandlerVRC7, 0x0F); return true;
		*/
	}
	return false;
}

void CChannelHandler2A03::ProcessChannel()
{
	// Default effects
	CChannelHandler::ProcessChannel();
	
	// Skip when DPCM
	if (m_iChannelID == CHANID_DPCM)
		return;
}

void CChannelHandler2A03::ResetChannel()
{
	CChannelHandler::ResetChannel();
	m_bEnvelopeLoop = true;		// // //
	m_bHardwareEnvelope = false;
	m_iLengthCounter = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Square 1 
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void CSquare1Chan::RefreshChannel()
{
	int Period = CalculatePeriod();
	int Volume = CalculateVolume();
	char DutyCycle = (m_iDutyPeriod & 0x03);

	unsigned char HiFreq = (Period & 0xFF);
	unsigned char LoFreq = (Period >> 8);
	
	if (m_bGate)		// // //
		WriteRegister(0x4000, (DutyCycle << 6) | (m_bEnvelopeLoop << 5) | (!m_bHardwareEnvelope << 4) | Volume);		// // //
	else {
		WriteRegister(0x4000, 0x30);
		m_iLastPeriod = 0xFFFF;
		return;
	}

	if (m_cSweep) {
		if (m_cSweep & 0x80) {
			WriteRegister(0x4001, m_cSweep);
			m_cSweep &= 0x7F;
			WriteRegister(0x4017, 0x80);	// Clear sweep unit
			WriteRegister(0x4017, 0x00);
			WriteRegister(0x4002, HiFreq);
			WriteRegister(0x4003, LoFreq + (m_iLengthCounter << 3));		// // //
			m_iLastPeriod = 0xFFFF;
		}
	}
	else {
		WriteRegister(0x4001, 0x08);
		//WriteRegister(0x4017, 0x80);	// Manually execute one APU frame sequence to kill the sweep unit
		//WriteRegister(0x4017, 0x00);
		WriteRegister(0x4002, HiFreq);
		
		if (LoFreq != (m_iLastPeriod >> 8) || m_bResetEnvelope)		// // //
			WriteRegister(0x4003, LoFreq + (m_iLengthCounter << 3));
	}

	m_iLastPeriod = Period;
	m_bResetEnvelope = false;		// // //
}

int CSquare1Chan::ConvertDuty(int Duty) const		// // //
{
	switch (m_iInstTypeCurrent) {
	case INST_VRC6:	return DUTY_2A03_FROM_VRC6[Duty & 0x07];
	case INST_N163:	return Duty;
	case INST_S5B:	return 0x02;
	default:		return Duty;
	}
}

void CSquare1Chan::ClearRegisters()
{
	WriteRegister(0x4000, 0x30);
	WriteRegister(0x4001, 0x08);
	WriteRegister(0x4002, 0x00);
	WriteRegister(0x4003, 0x00);
	m_iLastPeriod = 0xFFFF;
}

CString CSquare1Chan::GetCustomEffectString() const		// // //
{
	CString str = _T("");
	
	if (!m_bEnvelopeLoop)
		str.AppendFormat(_T(" E%02X"), m_iLengthCounter);
	if (!m_bEnvelopeLoop || m_bHardwareEnvelope)
		str.AppendFormat(_T(" EE%X"), !m_bEnvelopeLoop * 2 + m_bHardwareEnvelope);

	return str;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Square 2 
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void CSquare2Chan::RefreshChannel()
{
	int Period = CalculatePeriod();
	int Volume = CalculateVolume();
	char DutyCycle = (m_iDutyPeriod & 0x03);

	unsigned char HiFreq		= (Period & 0xFF);
	unsigned char LoFreq		= (Period >> 8);
	unsigned char LastLoFreq	= (m_iLastPeriod >> 8);
	
	if (m_bGate)		// // //
		WriteRegister(0x4004, (DutyCycle << 6) | (m_bEnvelopeLoop << 5) | (!m_bHardwareEnvelope << 4) | Volume);		// // //
	else {
		WriteRegister(0x4004, 0x30);
		m_iLastPeriod = 0xFFFF;
		return;
	}

	if (m_cSweep) {
		if (m_cSweep & 0x80) {
			WriteRegister(0x4005, m_cSweep);
			m_cSweep &= 0x7F;
			WriteRegister(0x4017, 0x80);		// Clear sweep unit
			WriteRegister(0x4017, 0x00);
			WriteRegister(0x4006, HiFreq);
			WriteRegister(0x4007, LoFreq + (m_iLengthCounter << 3));		// // //
			m_iLastPeriod = 0xFFFF;
		}
	}
	else {
		WriteRegister(0x4005, 0x08);
		//WriteRegister(0x4017, 0x80);
		//WriteRegister(0x4017, 0x00);
		WriteRegister(0x4006, HiFreq);
		
		if (LoFreq != LastLoFreq || m_bResetEnvelope)		// // //
			WriteRegister(0x4007, LoFreq + (m_iLengthCounter << 3));
	}

	m_iLastPeriod = Period;
	m_bResetEnvelope = false;		// // //
}

int CSquare2Chan::ConvertDuty(int Duty) const		// // //
{
	switch (m_iInstTypeCurrent) {
	case INST_VRC6:	return DUTY_2A03_FROM_VRC6[Duty & 0x07];
	case INST_N163:	return Duty;
	case INST_S5B:	return 0x02;
	default:		return Duty;
	}
}

void CSquare2Chan::ClearRegisters()
{
	WriteRegister(0x4004, 0x30);
	WriteRegister(0x4005, 0x08);
	WriteRegister(0x4006, 0x00);
	WriteRegister(0x4007, 0x00);
	m_iLastPeriod = 0xFFFF;
}

CString CSquare2Chan::GetCustomEffectString() const		// // //
{
	CString str = _T("");
	
	if (!m_bEnvelopeLoop)
		str.AppendFormat(_T(" E%02X"), m_iLengthCounter);
	if (!m_bEnvelopeLoop || m_bHardwareEnvelope)
		str.AppendFormat(_T(" EE%X"), !m_bEnvelopeLoop * 2 + m_bHardwareEnvelope);

	return str;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Triangle 
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTriangleChan::RefreshChannel()
{
	int Freq = CalculatePeriod();

	unsigned char HiFreq = (Freq & 0xFF);
	unsigned char LoFreq = (Freq >> 8);
	
	if (m_iInstVolume > 0 && m_iVolume > 0 && m_bGate) {
		WriteRegister(0x4008, (m_bEnvelopeLoop << 7) | m_iLinearCounter);		// // //
		WriteRegister(0x400A, HiFreq);
		if (m_bEnvelopeLoop || m_bResetEnvelope)		// // //
			WriteRegister(0x400B, LoFreq + (m_iLengthCounter << 3));
	}
	else
		WriteRegister(0x4008, 0);

	m_bResetEnvelope = false;		// // //
}

void CTriangleChan::ResetChannel()
{
	CChannelHandler2A03::ResetChannel();
	m_iLinearCounter = -1;
}

void CTriangleChan::HandleCustomEffects(int EffNum, int EffParam)		// // //
{
	CChannelHandler2A03::HandleCustomEffects(EffNum, EffParam);
	switch (EffNum) {
		case EF_VOLUME:
			if (m_iLinearCounter == -1)	m_iLinearCounter = 0x7F;
			break;
		case EF_NOTE_CUT:
			if (EffParam >= 0x80) {
				m_iLinearCounter = EffParam - 0x80;
				m_bEnvelopeLoop = false;
				m_bResetEnvelope = true;
			}
			else {
				m_bEnvelopeLoop = true;
			}
			break;
	}
}

void CTriangleChan::ClearRegisters()
{
	WriteRegister(0x4008, 0);
	WriteRegister(0x400A, 0);
	WriteRegister(0x400B, 0);
}

CString CTriangleChan::GetCustomEffectString() const		// // //
{
	CString str = _T("");
	
	if (m_iLinearCounter > -1)
		str.AppendFormat(_T(" S%02X"), m_iLinearCounter | 0x80);
	if (!m_bEnvelopeLoop)
		str.AppendFormat(_T(" E%02X"), m_iLengthCounter);
	if (!m_bEnvelopeLoop || m_bHardwareEnvelope)
		str.AppendFormat(_T(" EE%X"), !m_bEnvelopeLoop * 2 + m_bHardwareEnvelope);

	return str;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Noise
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void CNoiseChan::HandleNote(int Note, int Octave)
{
	int NewNote = (MIDI_NOTE(Octave, Note) & 0x0F) | 0x100;
	int NesFreq = TriggerNote(NewNote);

	// // // NesFreq = (NesFreq & 0x0F) | 0x10;

	// // // NewNote &= 0x0F;

	if (m_iPortaSpeed > 0 && m_iEffect == EF_PORTAMENTO) {
		if (m_iPeriod == 0)
			m_iPeriod = NesFreq;
		m_iPortaTo = NesFreq;
	}
	else
		m_iPeriod = NesFreq;

	m_bGate = true;

	m_iNote			= NewNote;
	m_iDutyPeriod	= m_iDefaultDuty;
	m_iInstVolume	= 0x0F;		// // //
}

/*
int CNoiseChan::CalculatePeriod() const
{
	return LimitPeriod(m_iPeriod - GetVibrato() + GetFinePitch() + GetPitch());
}
*/

void CNoiseChan::RefreshChannel()
{
	int Period = CalculatePeriod();
	int Volume = CalculateVolume();
	char NoiseMode = (m_iDutyPeriod & 0x01) << 7;

#ifdef NOISE_PITCH_SCALE
	Period = (Period >> 4) & 0x0F;
#else
	Period = Period & 0x0F;
#endif

	Period ^= 0x0F;
	
	if (m_bGate)		// // //
		WriteRegister(0x400C, (m_bEnvelopeLoop << 5) | (!m_bHardwareEnvelope << 4) | Volume);		// // //
	else {
		WriteRegister(0x400C, 0x30);
		return;
	}
	WriteRegister(0x400E, NoiseMode | Period);
	if (m_bEnvelopeLoop || m_bResetEnvelope)		// // //
		WriteRegister(0x400F, m_iLengthCounter << 3);

	m_bResetEnvelope = false;		// // //
}

void CNoiseChan::ClearRegisters()
{
	WriteRegister(0x400C, 0x30);
	WriteRegister(0x400E, 0);
	WriteRegister(0x400F, 0);
}

CString CNoiseChan::GetCustomEffectString() const		// // //
{
	CString str = _T("");

	if (!m_bEnvelopeLoop)
		str.AppendFormat(_T(" E%02X"), m_iLengthCounter);
	if (!m_bEnvelopeLoop || m_bHardwareEnvelope)
		str.AppendFormat(_T(" EE%X"), !m_bEnvelopeLoop * 2 + m_bHardwareEnvelope);

	return str;
}

int CNoiseChan::TriggerNote(int Note)
{
	// Clip range to 0-15
	/*
	if (Note > 0x0F)
		Note = 0x0F;
	if (Note < 0)
		Note = 0;
		*/

	RegisterKeyState(Note);

//	Note &= 0x0F;

#ifdef NOISE_PITCH_SCALE
	return (Note ^ 0x0F) << 4;
#else
	return Note | 0x100;		// // //
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// DPCM
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CDPCMChan::CDPCMChan(CSampleMem *pSampleMem) : 
	CChannelHandler2A03(), 
	m_pSampleMem(pSampleMem),
	m_bEnabled(false),
	m_bTrigger(false),
	m_cDAC(255),
	m_iRetrigger(0),
	m_iRetriggerCntr(0)
{ 
}

void CDPCMChan::HandleNoteData(stChanNote *pNoteData, int EffColumns)
{
	m_iCustomPitch = -1;
	m_iRetrigger = 0;

	if (pNoteData->Note != NONE) {
		m_iNoteCut = 0;
		m_iNoteRelease = 0;			// // //
	}

	CChannelHandler::HandleNoteData(pNoteData, EffColumns);
}

void CDPCMChan::HandleCustomEffects(int EffNum, int EffParam)
{
	switch (EffNum) {
	case EF_DAC:
		m_cDAC = EffParam & 0x7F;
		break;
	case EF_SAMPLE_OFFSET:
		m_iOffset = EffParam;
		break;
	case EF_DPCM_PITCH:
		m_iCustomPitch = EffParam;
		break;
	case EF_RETRIGGER:
//			if (NoteData->EffParam[i] > 0) {
			m_iRetrigger = EffParam + 1;
			if (m_iRetriggerCntr == 0)
				m_iRetriggerCntr = m_iRetrigger;
//			}
//			m_iEnableRetrigger = 1;
		break;
	case EF_NOTE_CUT:
		if (EffParam >= 0x80) break;		// // //
		m_iNoteCut = EffParam + 1;
		break;
	case EF_NOTE_RELEASE:		// // //
		if (EffParam >= 0x80) break;
		m_iNoteRelease = EffParam + 1;
		break;
	}
}

bool CDPCMChan::HandleInstrument(int Instrument, bool Trigger, bool NewInstrument)
{
	// Instruments are accessed in the note routine
	return true;
}

void CDPCMChan::HandleEmptyNote()
{
}

void CDPCMChan::HandleCut()
{
//	KillChannel();
	CutNote();
}

void CDPCMChan::HandleRelease()
{
	m_bRelease = true;
}

void CDPCMChan::HandleNote(int Note, int Octave)
{
	CFamiTrackerDoc *pDocument = m_pSoundGen->GetDocument();
	CInstrumentContainer<CInstrument2A03> instContainer(pDocument, m_iInstrument);
	CInstrument2A03 *pInstrument = instContainer();

	if (pInstrument == NULL)
		return;

	if (pInstrument->GetType() != INST_2A03)
		return;

	int SampleIndex = pInstrument->GetSample(Octave, Note - 1);

	if (SampleIndex > 0) {

		int Pitch = pInstrument->GetSamplePitch(Octave, Note - 1);
		m_iLoop = (Pitch & 0x80) >> 1;

		if (m_iCustomPitch != -1)
			Pitch = m_iCustomPitch;
	
		m_iLoopOffset = pInstrument->GetSampleLoopOffset(Octave, Note - 1);

		const CDSample *pDSample = pDocument->GetSample(SampleIndex - 1);

		int SampleSize = pDSample->GetSize();

		if (SampleSize > 0) {
			m_pSampleMem->SetMem(pDSample->GetData(), SampleSize);
			m_iPeriod = Pitch & 0x0F;
			m_iSampleLength = (SampleSize >> 4) - (m_iOffset << 2);
			m_iLoopLength = SampleSize - m_iLoopOffset;
			m_bEnabled = true;
			m_bTrigger = true;
			m_bGate = true;

			// Initial delta counter value
			unsigned char Delta = pInstrument->GetSampleDeltaValue(Octave, Note - 1);
			
			if (Delta != 255 && m_cDAC == 255)
				m_cDAC = Delta;

			m_iRetriggerCntr = m_iRetrigger;
		}
	}

	RegisterKeyState((Note - 1) + (Octave * NOTE_RANGE));
}

void CDPCMChan::RefreshChannel()
{
	if (m_cDAC != 255) {
		WriteRegister(0x4011, m_cDAC);
		m_cDAC = 255;
	}

	if (m_iRetrigger != 0) {
		m_iRetriggerCntr--;
		if (m_iRetriggerCntr == 0) {
			m_iRetriggerCntr = m_iRetrigger;
			m_bEnabled = true;
			m_bTrigger = true;
		}
	}

	if (m_bRelease) {
		// Release command
		WriteRegister(0x4015, 0x0F);
		m_bEnabled = false;
		m_bRelease = false;
	}

/*	
	if (m_bRelease) {
		// Release loop flag
		m_bRelease = false;
		WriteRegister(0x4010, 0x00 | (m_iPeriod & 0x0F));
		return;
	}
*/	

	if (!m_bEnabled)
		return;

	if (!m_bGate) {
		// Cut sample
		WriteRegister(0x4015, 0x0F);

		if (!theApp.GetSettings()->General.bNoDPCMReset || theApp.IsPlaying()) {
			WriteRegister(0x4011, 0);	// regain full volume for TN
		}

		m_bEnabled = false;		// don't write to this channel anymore
	}
	else if (m_bTrigger) {
		// Start playing the sample
		WriteRegister(0x4010, (m_iPeriod & 0x0F) | m_iLoop);
		WriteRegister(0x4012, m_iOffset);							// load address, start at $C000
		WriteRegister(0x4013, m_iSampleLength);						// length
		WriteRegister(0x4015, 0x0F);
		WriteRegister(0x4015, 0x1F);								// fire sample

		// Loop offset
		if (m_iLoopOffset > 0) {
			WriteRegister(0x4012, m_iLoopOffset);
			WriteRegister(0x4013, m_iLoopLength);
		}

		m_bTrigger = false;
	}
}

void CDPCMChan::ClearRegisters()
{
	WriteRegister(0x4015, 0x0F);

	WriteRegister(0x4010, 0);	
	WriteRegister(0x4011, 0);
	WriteRegister(0x4012, 0);
	WriteRegister(0x4013, 0);

	m_iOffset = 0;
	m_cDAC = 255;
}

CString CDPCMChan::GetCustomEffectString() const		// // //
{
	CString str = _T("");

	if (m_iOffset)
		str.AppendFormat(_T(" Y%02X"), m_iOffset);

	return str;
}
