#pragma once

#include "vgui_TeamFortressViewport.h"
#include "vgui_slider2.h"
#include "vgui_checkbutton2.h"
#include <VGUI_Label.h>
#include <VGUI_IntChangeSignal.h>

class CAGSettingsPanel : public CMenuPanel, public vgui::IntChangeSignal, public vgui::ICheckButton2Handler
{
private:
	vgui::CCheckButton2* m_pForceTeammateCheckbox;
	vgui::CCheckButton2* m_pForceEnemyCheckbox;
	vgui::CCheckButton2* m_pExplosionsCheckbox;
	vgui::CCheckButton2* m_pDiscordRpcCheckbox;
	vgui::CCheckButton2* m_pItemTimersCheckbox;
	vgui::CCheckButton2* m_pDamageNumbersCheckbox;

	vgui::Label* m_pFootstepVolLabel;
	vgui::Slider2* m_pFootstepVolSlider;

	CommandButton* m_pCloseButton;

	void UpdateLabelText(vgui::Label* label, const char* name, int value);

public:
	CAGSettingsPanel(int iTrans, int iRemoveMe, int x, int y, int wide, int tall);

	virtual void Open(void) override;
	virtual void Close(void) override;
	virtual void intChanged(int value, vgui::Panel* panel) override;
	virtual void StateChanged(vgui::CCheckButton2* pButton) override;
};
