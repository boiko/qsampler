// qsamplerChannelStrip.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
/****************************************************************************
   Copyright (C) 2004, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include <qvalidator.h>
#include <qmessagebox.h>
#include <qfileinfo.h>
#include <qtooltip.h>
#include <qpopupmenu.h>

#include <math.h>

#include "qsamplerMainForm.h"
#include "qsamplerChannelForm.h"

#include "config.h"


// Kind of constructor.
void qsamplerChannelStrip::init (void)
{
    // Initialize locals.
    m_pMainForm    = NULL;
    m_pChannel     = NULL;
    m_iDirtyChange = 0;

    // Try to restore normal window positioning.
    adjustSize();
}


// Kind of destructor.
void qsamplerChannelStrip::destroy (void)
{
    // Destroy existing channel descriptor.
    if (m_pChannel)
        delete m_pChannel;
    m_pChannel = NULL;
}


// Channel strip setup formal initializer.
void qsamplerChannelStrip::setup ( qsamplerMainForm *pMainForm, int iChannelID )
{
    // Set main form reference.
    m_pMainForm = pMainForm;
    
    // Destroy any previous channel descriptor.
    if (m_pChannel)
        delete m_pChannel;

    // Create a new one...
    m_pChannel = new qsamplerChannel(pMainForm);
    // And set appropriate settings.
    if (m_pChannel) {
        m_pChannel->setChannelID(iChannelID);
        m_iDirtyChange = 0;
    }
    
    // Stabilize this around.
    updateChannelInfo();
}

// Channel secriptor accessor.
qsamplerChannel *qsamplerChannelStrip::channel (void)
{
    return m_pChannel;
}


// Messages view font accessors.
QFont qsamplerChannelStrip::displayFont (void)
{
    return EngineNameTextLabel->font();
}

void qsamplerChannelStrip::setDisplayFont ( const QFont & font )
{
    EngineNameTextLabel->setFont(font);
    MidiPortChannelTextLabel->setFont(font);
    InstrumentNameTextLabel->setFont(font);
    InstrumentStatusTextLabel->setFont(font);
}


// Channel setup dialog slot.
void qsamplerChannelStrip::channelSetup (void)
{
    showChannelSetup(false);
}


// Channel setup dialog.
void qsamplerChannelStrip::showChannelSetup ( bool bNew )
{
    qsamplerChannelForm *pChannelForm = new qsamplerChannelForm(this);
    if (pChannelForm) {
        pChannelForm->setup(m_pChannel, bNew);
        if (pChannelForm->exec()) {
            updateChannelInfo();
            emit channelChanged(this);
        }
        delete pChannelForm;
    }
}


// Update whole channel info state.
void qsamplerChannelStrip::updateChannelInfo (void)
{
    if (m_pChannel == NULL)
        return;
        
    // Update strip caption.
    QString sText = tr("Channel %1").arg(m_pChannel->channelID());
    setCaption(sText);
    ChannelSetupPushButton->setText(sText);

    // Check if we're up and connected.
    if (m_pChannel->client() == NULL)
        return;

    // Read actual channel information.
    m_pChannel->updateChannelInfo();

    // Set some proper display values.
    const QString sIndent = " ";

    // Engine name...
    if (m_pChannel->engineName().isEmpty())
        EngineNameTextLabel->setText(sIndent + tr("(No engine)"));
    else
        EngineNameTextLabel->setText(sIndent + m_pChannel->engineName());

    // Instrument name...
    if (m_pChannel->instrumentFile().isEmpty())
        InstrumentNameTextLabel->setText(sIndent + tr("(No instrument)"));
    else
        InstrumentNameTextLabel->setText(sIndent + QString("%1 [%2]")
            .arg(QFileInfo(m_pChannel->instrumentFile()).fileName()).arg(m_pChannel->instrumentNr()));

    // Instrument status...
    int iInstrumentStatus = m_pChannel->instrumentStatus();
    if (iInstrumentStatus < 0) {
        InstrumentStatusTextLabel->setPaletteForegroundColor(Qt::red);
        InstrumentStatusTextLabel->setText(tr("ERR%1").arg(iInstrumentStatus));
    } else {
        InstrumentStatusTextLabel->setPaletteForegroundColor(iInstrumentStatus < 100 ? Qt::yellow : Qt::green);
        InstrumentStatusTextLabel->setText(QString::number(iInstrumentStatus) + "%");
    }

    // MIDI Port/Channel...
    if (m_pChannel->midiChannel() > 0)
        MidiPortChannelTextLabel->setText(QString("%1 / %2").arg(m_pChannel->midiPort()).arg(m_pChannel->midiChannel()));
    else
        MidiPortChannelTextLabel->setText(QString("%1 / *").arg(m_pChannel->midiPort()));

    // And update the both GUI volume elements.
    updateChannelVolume();
}


// Do the dirty volume change.
void qsamplerChannelStrip::updateChannelVolume (void)
{
    if (m_pChannel == NULL)
        return;

    // Convert...
#ifdef CONFIG_ROUND
    int iVolume = (int) ::round(100.0 * m_pChannel->volume());
#else
    double fIPart = 0.0;
    double fFPart = ::modf(100.0 * m_pChannel->volume(), &fIPart);
    int iVolume = (int) fIPart;
    if (fFPart >= +0.5)
        iVolume++;
    else
    if (fFPart <= -0.5)
        iVolume--;
#endif

    // And clip...
    if (iVolume < 0)
        iVolume = 0;

    // Flag it here, to avoid infinite recursion.
    m_iDirtyChange++;
    VolumeSlider->setValue(iVolume);
    VolumeSpinBox->setValue(iVolume);
    m_iDirtyChange--;
}


// Update whole channel usage state.
void qsamplerChannelStrip::updateChannelUsage (void)
{
    if (m_pChannel == NULL)
        return;
    if (m_pChannel->client() == NULL)
        return;

    // Conditionally update whole channel status info.
    if (m_pChannel->instrumentStatus() >= 0 && m_pChannel->instrumentStatus() < 100) {
        updateChannelInfo();
        // Once we get a complete instrument load, try a implied reset channel....
        if (m_pChannel->instrumentStatus() == 100)
            m_pChannel->resetChannel();
    }
    // Leave, if we still have an erroneus or incomplete instrument load.
    if (m_pChannel->instrumentStatus() < 100)
        return;

    // Get current channel voice count.
    int iVoiceCount  = ::lscp_get_channel_voice_count(m_pChannel->client(), m_pChannel->channelID());
    // Get current stream count.
    int iStreamCount = ::lscp_get_channel_stream_count(m_pChannel->client(), m_pChannel->channelID());
    // Get current channel buffer fill usage.
    // As benno has suggested this is the percentage usage
    // of the least filled buffer stream...
    int iStreamUsage = ::lscp_get_channel_stream_usage(m_pChannel->client(), m_pChannel->channelID());;

    // Update the GUI elements...
    StreamUsageProgressBar->setProgress(iStreamUsage);
    StreamVoiceCountTextLabel->setText(QString("%1 / %2").arg(iStreamCount).arg(iVoiceCount));
}


// Volume change slot.
void qsamplerChannelStrip::volumeChanged ( int iVolume )
{
    if (m_pChannel == NULL)
        return;

    // Avoid recursion.
    if (m_iDirtyChange > 0)
        return;

    // Convert and clip.
    float fVolume = (float) iVolume / 100.0;
    if (fVolume < 0.001)
        fVolume = 0.0;

    // Update the GUI elements.
    if (m_pChannel->setVolume(fVolume)) {
        updateChannelVolume();
        emit channelChanged(this);
    }
}


// Context menu event handler.
void qsamplerChannelStrip::contextMenuEvent( QContextMenuEvent *pEvent )
{
    if (m_pMainForm == NULL)
        return;
        
    // We'll just show up the main form's edit menu.
    m_pMainForm->contextMenuEvent(pEvent);
}


// Maximum volume slider accessors.
void qsamplerChannelStrip::setMaxVolume ( int iMaxVolume )
{
    m_iDirtyChange++;
    VolumeSlider->setRange(0, iMaxVolume);
    VolumeSpinBox->setRange(0, iMaxVolume);
    m_iDirtyChange--;
}


// end of qsamplerChannelStrip.ui.h