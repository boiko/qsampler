// qsamplerDeviceForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <qmessagebox.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qlistbox.h>

#include "qsamplerMainForm.h"

#include "config.h"


// Kind of constructor.
void qsamplerDeviceForm::init (void)
{
	// Initialize locals.
	m_pMainForm   = (qsamplerMainForm *) QWidget::parentWidget();
	m_pClient     = NULL;
	m_iDirtySetup = 0;
	m_bNewDevice  = false;
	m_deviceType  = qsamplerDevice::None;
	m_pAudioItems = NULL;
	m_pMidiItems  = NULL;

	// This an outsider (from designer), but rather important.
	QObject::connect(DeviceParamTable, SIGNAL(valueChanged(int,int)),
		this, SLOT(changeValue(int,int)));
	
	// Try to restore normal window positioning.
	adjustSize();
}


// Kind of destructor.
void qsamplerDeviceForm::destroy (void)
{
}


// Notify our parent that we're emerging.
void qsamplerDeviceForm::showEvent ( QShowEvent *pShowEvent )
{
	if (m_pMainForm)
		m_pMainForm->stabilizeForm();

	stabilizeForm();

	QWidget::showEvent(pShowEvent);
}


// Notify our parent that we're closing.
void qsamplerDeviceForm::hideEvent ( QHideEvent *pHideEvent )
{
	QWidget::hideEvent(pHideEvent);

	if (m_pMainForm)
		m_pMainForm->stabilizeForm();
}


// Device configuration dialog setup formal initializer.
void qsamplerDeviceForm::setClient ( lscp_client_t *pClient )
{
	// If it has not changed, do nothing.
	if (m_pClient && m_pClient == pClient)
		return;

	// Set new reference.
	m_pClient = pClient;
	
	// OK. Do a whole refresh around.
	refreshDevices();
}


// Create a new device from current table view.
void qsamplerDeviceForm::createDevice (void)
{
	QListViewItem *pItem = DeviceListView->selectedItem();
	if (pItem == NULL || pItem->rtti() != QSAMPLER_DEVICE_ITEM)
		return;

	qsamplerDevice& device = ((qsamplerDeviceItem *) pItem)->device();

	// Build the parameter list...
	qsamplerDeviceParamMap& params = device.params();
	lscp_param_t *pParams = new lscp_param_t [params.count() + 1];
	int iParam = 0;
	qsamplerDeviceParamMap::ConstIterator iter;
	for (iter = params.begin(); iter != params.end(); ++iter) {
		pParams[iParam].key   = (char *) iter.key().latin1();
		pParams[iParam].value = (char *) iter.data().value.latin1();
		++iParam;
	}
	// Null terminated.
	pParams[iParam].key   = NULL;
	pParams[iParam].value = NULL;

	// Now it depends on the device type...
	qsamplerDeviceItem *pRootItem = NULL;
	int iDeviceID = -1;
	switch (device.deviceType()) {
	case qsamplerDevice::Audio:
		pRootItem = m_pAudioItems;
		if ((iDeviceID = ::lscp_create_audio_device(m_pClient,
				device.driverName().latin1(), pParams)) < 0)
			m_pMainForm->appendMessagesClient("lscp_create_audio_device");
		break;
	case qsamplerDevice::Midi:
		pRootItem = m_pMidiItems;
		if ((iDeviceID = ::lscp_create_midi_device(m_pClient,
				device.driverName().latin1(), pParams)) < 0)
			m_pMainForm->appendMessagesClient("lscp_create_midi_device");
		break;
	case qsamplerDevice::None:
		break;
	}

	// Free used parameter array.
	delete pParams;

	// We're on to create the new device item.
	if (iDeviceID >= 0) {
		// Append the new device item.
		qsamplerDeviceItem *pDeviceItem = new qsamplerDeviceItem(pRootItem,
			m_pClient, device.deviceType(), iDeviceID);
		// Just make it the new selection...
		DeviceListView->setSelected(pDeviceItem, true);
		// Done.
		m_pMainForm->appendMessages(pDeviceItem->device().deviceName() + ' '
			+ tr("created."));
		// Main session should be marked dirty.
		m_pMainForm->sessionDirty();
	}
}


// Delete current device in table view.
void qsamplerDeviceForm::deleteDevice (void)
{
	QListViewItem *pItem = DeviceListView->selectedItem();
	if (pItem == NULL || pItem->rtti() != QSAMPLER_DEVICE_ITEM)
		return;

	qsamplerDevice& device = ((qsamplerDeviceItem *) pItem)->device();

	// Prompt user if this is for real...
	qsamplerOptions *pOptions = m_pMainForm->options();
	if (pOptions && pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this, tr("Warning"),
			tr("Delete %1 device:\n\n"
			"%2\n\n"
			"Are you sure?")
			.arg(device.deviceTypeName())
			.arg(device.deviceName()),
			tr("OK"), tr("Cancel")) > 0)
			return;
	}

	// Now it depends on the device type...
	lscp_status_t ret = LSCP_FAILED;
	switch (device.deviceType()) {
	case qsamplerDevice::Audio:
		if ((ret = ::lscp_destroy_audio_device(m_pClient,
				device.deviceID())) != LSCP_OK)
			m_pMainForm->appendMessagesClient("lscp_destroy_audio_device");
		break;
	case qsamplerDevice::Midi:
		if ((ret = ::lscp_destroy_midi_device(m_pClient,
				device.deviceID())) != LSCP_OK)
			m_pMainForm->appendMessagesClient("lscp_destroy_midi_device");
		break;
	case qsamplerDevice::None:
		break;
	}

	// Show result.
	if (ret == LSCP_OK) {
		// Show log message before loosing it.
		m_pMainForm->appendMessages(device.deviceName() + ' '
			+ tr("deleted."));
		// Done.
		delete pItem;
		// Main session should be marked dirty.
		m_pMainForm->sessionDirty();
	}
}


// Refresh all device list and views.
void qsamplerDeviceForm::refreshDevices (void)
{
	// Avoid nested changes.
	m_iDirtySetup++;

	//
	// (Re)Load complete device configuration data ...
	//
	m_pAudioItems = NULL;
	m_pMidiItems = NULL;
	DeviceListView->clear();
	if (m_pClient) {
		int *piDeviceIDs;
		// Grab and pop Audio devices...
		m_pAudioItems = new qsamplerDeviceItem(DeviceListView, m_pClient,
			qsamplerDevice::Audio);
		if (m_pAudioItems) {
			m_pAudioItems->setText(0, tr("Audio"));
			piDeviceIDs = qsamplerDevice::getDevices(m_pClient, qsamplerDevice::Audio);
			for (int i = 0; piDeviceIDs && piDeviceIDs[i] >= 0; i++) {
				new qsamplerDeviceItem(m_pAudioItems, m_pClient,
					qsamplerDevice::Audio, piDeviceIDs[i]);
			}
			m_pAudioItems->setOpen(true);
		}
		// Grab and pop MIDI devices...
		m_pMidiItems = new qsamplerDeviceItem(DeviceListView, m_pClient,
			qsamplerDevice::Midi);
		if (m_pMidiItems) {
			m_pMidiItems->setText(0, tr("MIDI"));
			piDeviceIDs = qsamplerDevice::getDevices(m_pClient, qsamplerDevice::Midi);
			for (int i = 0; piDeviceIDs && piDeviceIDs[i] >= 0; i++) {
				new qsamplerDeviceItem(m_pMidiItems, m_pClient,
					qsamplerDevice::Midi, piDeviceIDs[i]);
			}
			m_pMidiItems->setOpen(true);
		}
	}

	// Done.
	m_iDirtySetup--;

	// Show something.
	selectDevice();
}


// Driver selection slot.
void qsamplerDeviceForm::selectDriver ( const QString& sDriverName )
{
	if (m_iDirtySetup > 0)
		return;

	//
	//  Driver name has changed for a new device...
	//

	QListViewItem *pItem = DeviceListView->selectedItem();
	if (pItem == NULL || pItem->rtti() != QSAMPLER_DEVICE_ITEM)
		return;

	qsamplerDevice& device = ((qsamplerDeviceItem *) pItem)->device();

	// Driver change is only valid for scratch devices...
	if (m_bNewDevice) {
		m_iDirtySetup++;
		device.setDriver(m_pClient, sDriverName);
		DeviceParamTable->refresh(device);
		m_iDirtySetup--;
		// Done.
		stabilizeForm();
	}
}


// Device selection slot.
void qsamplerDeviceForm::selectDevice (void)
{
	if (m_iDirtySetup > 0)
		return;

	//
	//  Device selection has changed...
	//

	QListViewItem *pItem = DeviceListView->selectedItem();
	if (pItem == NULL || pItem->rtti() != QSAMPLER_DEVICE_ITEM) {
		m_deviceType = qsamplerDevice::None;
		DeviceNameTextLabel->setText(QString::null);
		DeviceParamTable->setNumRows(0);
		stabilizeForm();
		return;
	}

	m_iDirtySetup++;
	qsamplerDevice& device = ((qsamplerDeviceItem *) pItem)->device();

	// Flag whether this is a new device.
	m_bNewDevice = (device.deviceID() < 0);

	// Fill the device/driver heading...
	DeviceNameTextLabel->setText(device.deviceTypeName() + ' ' + device.deviceName());
	// The driver combobox is only rebuilt if device type has changed...
	if (device.deviceType() != m_deviceType) {
		DriverNameComboBox->clear();
		DriverNameComboBox->insertStringList(
			qsamplerDevice::getDrivers(m_pClient, device.deviceType()));
		m_deviceType = device.deviceType();
	}
	// Do we need a driver name?
	if (m_bNewDevice || device.driverName().isEmpty())
		device.setDriver(m_pClient, DriverNameComboBox->currentText());
	const QString& sDriverName = device.driverName();
	if (DriverNameComboBox->listBox()->findItem(sDriverName, Qt::ExactMatch) == NULL)
		DriverNameComboBox->insertItem(sDriverName);
	DriverNameComboBox->setCurrentText(sDriverName);
	DriverNameTextLabel->setEnabled(m_bNewDevice);
	DriverNameComboBox->setEnabled(m_bNewDevice);
	// Fill the device parameter table...
	DeviceParamTable->refresh(device);
	// Done.
	m_iDirtySetup--;
	stabilizeForm();
}


// parameter value change slot.
void qsamplerDeviceForm::changeValue ( int iRow, int iCol )
{
	if (m_iDirtySetup > 0)
		return;
	if (iRow < 0 || iCol < 0)
		return;
		
	//
	//  Device parameter change...
	//

	QListViewItem *pItem = DeviceListView->selectedItem();
	if (pItem == NULL || pItem->rtti() != QSAMPLER_DEVICE_ITEM)
		return;

	m_iDirtySetup++;
	qsamplerDevice& device = ((qsamplerDeviceItem *) pItem)->device();

	// Table 1st column has the parameter name;
	qsamplerDeviceParamMap& params = device.params();
	const QString sParam = DeviceParamTable->text(iRow, 0);
	const QString sValue = DeviceParamTable->text(iRow, iCol);
	params[sParam].value = sValue;

	// Set proper device parameter, on existing device ...
	if (device.deviceID() >= 0) {
		// Prepare parameter struct.
		lscp_param_t param;
		param.key   = (char *) sParam.latin1();
		param.value = (char *) sValue.latin1();
		// Now it depends on the device type...
		lscp_status_t ret = LSCP_FAILED;
		switch (device.deviceType()) {
		case qsamplerDevice::Audio:
			if ((ret = ::lscp_set_audio_device_param(m_pClient,
					device.deviceID(), &param)) != LSCP_OK)
				m_pMainForm->appendMessagesClient("lscp_set_audio_device_param");
			break;
		case qsamplerDevice::Midi:
			if ((ret = ::lscp_set_midi_device_param(m_pClient,
					device.deviceID(), &param)) != LSCP_OK)
				m_pMainForm->appendMessagesClient("lscp_set_midi_device_param");
			break;
		case qsamplerDevice::None:
			break;
		}
		// Show result.
		if (ret == LSCP_OK) {
			m_pMainForm->appendMessages(device.deviceName() + ' '
				+ QString("%1: %2.").arg(sParam).arg(sValue));
		}
	}

	// Done.
	m_iDirtySetup--;
	stabilizeForm();
	// Main session should be dirtier...
	m_pMainForm->sessionDirty();
}


// Stabilize current form state.
void qsamplerDeviceForm::stabilizeForm (void)
{
	QListViewItem *pItem = DeviceListView->selectedItem();
	bool bEnabled = (pItem != NULL);
	DeviceNameTextLabel->setEnabled(bEnabled && !m_bNewDevice);
	DriverNameTextLabel->setEnabled(bEnabled &&  m_bNewDevice);
	DriverNameComboBox->setEnabled(bEnabled && m_bNewDevice);
	DeviceParamTable->setEnabled(bEnabled);
	CreateDevicePushButton->setEnabled(bEnabled ||  m_bNewDevice);
	DeleteDevicePushButton->setEnabled(bEnabled && !m_bNewDevice);
}


// end of qsamplerDeviceForm.ui.h