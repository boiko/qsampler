// qsamplerInstrument.cpp
//
/****************************************************************************
   Copyright (C) 2004-2019, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2007, Christian Schoenebeck

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qsamplerAbout.h"
#include "qsamplerInstrument.h"
#include "qsamplerUtilities.h"

#include "qsamplerOptions.h"
#include "qsamplerMainForm.h"


namespace QSampler {

//-------------------------------------------------------------------------
// QSampler::Instrument - MIDI instrument map structure.
//

// Constructor.
Instrument::Instrument ( int iMap, int iBank, int iProg )
{
	m_iMap          = iMap;
	m_iBank         = iBank;
	m_iProg         = iProg;
	m_iInstrumentNr = 0;
	m_fVolume       = 1.0f;
	m_iLoadMode     = 0;
}

// Default destructor.
Instrument::~Instrument (void)
{
}


// Instrument accessors.
void Instrument::setMap ( int iMap )
{
	m_iMap = iMap;
}

int Instrument::map (void) const
{
	return m_iMap;
}


void Instrument::setBank ( int iBank )
{
	m_iBank = iBank;
}

int Instrument::bank (void) const
{
	return m_iBank;
}


void Instrument::setProg ( int iProg )
{
	m_iProg = iProg;
}

int Instrument::prog (void) const
{
	return m_iProg;
}


void Instrument::setName ( const QString& sName )
{
	m_sName = sName;
}

const QString& Instrument::name (void) const
{
	return m_sName;
}


void Instrument::setEngineName ( const QString& sEngineName )
{
	m_sEngineName = sEngineName;
}

const QString& Instrument::engineName (void) const
{
	return m_sEngineName;
}


void Instrument::setInstrumentFile ( const QString& sInstrumentFile )
{
	m_sInstrumentFile = sInstrumentFile;
}

const QString& Instrument::instrumentFile (void) const
{
	return m_sInstrumentFile;
}


const QString& Instrument::instrumentName (void) const
{
	return m_sInstrumentName;
}


void Instrument::setInstrumentNr ( int iInstrumentNr )
{
	m_iInstrumentNr = iInstrumentNr;
}

int Instrument::instrumentNr (void) const
{
	return m_iInstrumentNr;
}


void Instrument::setVolume ( float fVolume )
{
	m_fVolume = fVolume;
}

float Instrument::volume (void) const
{
	return m_fVolume;
}


void Instrument::setLoadMode ( int iLoadMode )
{
	m_iLoadMode = iLoadMode;
}

int Instrument::loadMode (void) const
{
	return m_iLoadMode;
}


// Sync methods.
bool Instrument::mapInstrument (void)
{
#ifdef CONFIG_MIDI_INSTRUMENT

	MainForm *pMainForm = MainForm::getInstance();
	if (pMainForm == nullptr)
		return false;
	if (pMainForm->client() == nullptr)
		return false;

	if (m_iMap < 0 || m_iBank < 0 || m_iProg < 0)
		return false;

	lscp_midi_instrument_t instr;

	instr.map  = m_iMap;
	instr.bank = (m_iBank & 0x0fff);
	instr.prog = (m_iProg & 0x7f);

	lscp_load_mode_t load_mode;
	switch (m_iLoadMode) {
		case 3:
			load_mode = LSCP_LOAD_PERSISTENT;
			break;
		case 2:
			load_mode = LSCP_LOAD_ON_DEMAND_HOLD;
			break;
		case 1:
			load_mode = LSCP_LOAD_ON_DEMAND;
			break;
		case 0:
		default:
			load_mode = LSCP_LOAD_DEFAULT;
			break;
	}

	if (::lscp_map_midi_instrument(pMainForm->client(), &instr,
			m_sEngineName.toUtf8().constData(),
			qsamplerUtilities::lscpEscapePath(
				m_sInstrumentFile).toUtf8().constData(),
			m_iInstrumentNr, m_fVolume, load_mode,
			m_sName.toUtf8().constData()) != LSCP_OK) {
		pMainForm->appendMessagesClient("lscp_map_midi_instrument");
		return false;
	}

	return true;

#else

	return false;

#endif
}


bool Instrument::unmapInstrument (void)
{
#ifdef CONFIG_MIDI_INSTRUMENT

	if (m_iMap < 0 || m_iBank < 0 || m_iProg < 0)
		return false;

	MainForm *pMainForm = MainForm::getInstance();
	if (pMainForm == nullptr)
		return false;
	if (pMainForm->client() == nullptr)
		return false;

	lscp_midi_instrument_t instr;

	instr.map  = m_iMap;
	instr.bank = (m_iBank & 0x0fff);
	instr.prog = (m_iProg & 0x7f);

	if (::lscp_unmap_midi_instrument(pMainForm->client(), &instr) != LSCP_OK) {
		pMainForm->appendMessagesClient("lscp_unmap_midi_instrument");
		return false;
	}

	return true;

#else

	return false;

#endif
}


bool Instrument::getInstrument (void)
{
#ifdef CONFIG_MIDI_INSTRUMENT

	if (m_iMap < 0 || m_iBank < 0 || m_iProg < 0)
		return false;

	MainForm *pMainForm = MainForm::getInstance();
	if (pMainForm == nullptr)
		return false;
	if (pMainForm->client() == nullptr)
		return false;

	lscp_midi_instrument_t instr;

	instr.map  = m_iMap;
	instr.bank = (m_iBank & 0x0fff);
	instr.prog = (m_iProg & 0x7f);

	lscp_midi_instrument_info_t *pInstrInfo
		= ::lscp_get_midi_instrument_info(pMainForm->client(), &instr);
	if (pInstrInfo == nullptr) {
		pMainForm->appendMessagesClient("lscp_get_midi_instrument_info");
		return false;
	}

	m_sName = qsamplerUtilities::lscpEscapedTextToRaw(pInstrInfo->name);
	m_sEngineName = pInstrInfo->engine_name;
	m_sInstrumentName = qsamplerUtilities::lscpEscapedTextToRaw(
		pInstrInfo->instrument_name);
	m_sInstrumentFile = qsamplerUtilities::lscpEscapedPathToPosix(
		pInstrInfo->instrument_file);
	m_iInstrumentNr = pInstrInfo->instrument_nr;
	m_fVolume = pInstrInfo->volume;

	switch (pInstrInfo->load_mode) {
		case LSCP_LOAD_PERSISTENT:
			m_iLoadMode = 3;
			break;
		case LSCP_LOAD_ON_DEMAND_HOLD:
			m_iLoadMode = 2;
			break;
		case LSCP_LOAD_ON_DEMAND:
			m_iLoadMode = 1;
			break;
		case LSCP_LOAD_DEFAULT:
		default:
			m_iLoadMode = 0;
			break;
	}

	// Fix something.
	if (m_sName.isEmpty())
		m_sName = m_sInstrumentName;

	return true;

#else

	return false;

#endif
}


// Instrument map name enumerator.
QStringList Instrument::getMapNames (void)
{
	QStringList maps;

	MainForm *pMainForm = MainForm::getInstance();
	if (pMainForm == nullptr)
		return maps;
	if (pMainForm->client() == nullptr)
		return maps;

#ifdef CONFIG_MIDI_INSTRUMENT
	int *piMaps = ::lscp_list_midi_instrument_maps(pMainForm->client());
	if (piMaps == nullptr) {
		if (::lscp_client_get_errno(pMainForm->client()))
			pMainForm->appendMessagesClient("lscp_list_midi_instruments");
	} else {
		for (int iMap = 0; piMaps[iMap] >= 0; iMap++) {
			const QString& sMapName = getMapName(piMaps[iMap]);
			if (!sMapName.isEmpty())
				maps.append(sMapName);
		}
	}
#endif

	return maps;
}

// Instrument map name enumerator.
QString Instrument::getMapName ( int iMidiMap )
{
	QString sMapName;

	MainForm *pMainForm = MainForm::getInstance();
	if (pMainForm == nullptr)
		return sMapName;
	if (pMainForm->client() == nullptr)
		return sMapName;

#ifdef CONFIG_MIDI_INSTRUMENT
	const char *pszMapName
		= ::lscp_get_midi_instrument_map_name(pMainForm->client(), iMidiMap);
	if (pszMapName == nullptr) {
		pszMapName = " -";
		if (::lscp_client_get_errno(pMainForm->client()))
			pMainForm->appendMessagesClient("lscp_get_midi_instrument_name");
	}
	sMapName = QString("%1 - %2").arg(iMidiMap)
		.arg(qsamplerUtilities::lscpEscapedTextToRaw(pszMapName));
#endif

	return sMapName;
}

} // namespace QSampler

// end of qsamplerInstrument.cpp
