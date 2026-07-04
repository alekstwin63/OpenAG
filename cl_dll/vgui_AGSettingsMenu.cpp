#include "hud.h"
#include "cl_util.h"
#undef min
#undef max
#include "vgui_AGSettingsMenu.h"
#include "vgui_int.h"
#include <VGUI_Font.h>
#include <VGUI_ActionSignal.h>
#include <VGUI_IntChangeSignal.h>
#include <cstdio>
#include <cstring>

using namespace vgui;

CAGSettingsPanel::CAGSettingsPanel(int iTrans, int iRemoveMe, int x, int y, int wide, int tall)
	: CMenuPanel(iTrans, iRemoveMe, x, y, wide, tall)
{
	setBgColor(0, 0, 0, 180);

	// Schemes
	CSchemeManager* pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle("Title Font");
	SchemeHandle_t hTextScheme = pSchemes->getSchemeHandle("Briefing Text");

	vgui::Font* pTitleFont = pSchemes->getFont(hTitleScheme);
	vgui::Font* pTextFont = pSchemes->getFont(hTextScheme);

	int r, g, b, a;
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);

	// Title Label
	vgui::Label* pTitle = new vgui::Label("AG SETTINGS PANEL", XRES(15), YRES(10), XRES(230), YRES(20));
	pTitle->setParent(this);
	if (pTitleFont)
		pTitle->setFont(pTitleFont);
	pTitle->setFgColor(r, g, b, a);
	pTitle->setBgColor(0, 0, 0, 255);
	pTitle->setContentAlignment(vgui::Label::a_center);

	int curY = YRES(35);
	int ctrlW = XRES(230);
	int ctrlX = XRES(15);

	// Checkbox layout helper
	auto createCheckbox = [&](const char* text, bool checked, vgui::CCheckButton2*& member) {
		member = new vgui::CCheckButton2();
		member->setParent(this);
		member->SetImages("gfx/vgui/checked.tga", "gfx/vgui/unchecked.tga");
		member->SetText(text);
		member->SetChecked(checked);
		member->SetHandler(this);
		member->setPos(ctrlX, curY);
		if (pTextFont)
			member->SetTextColor(255, 255, 255, 0); // white foreground
		curY += YRES(22);
	};

	// Retrieve CVar states
	cvar_t* pCvarTeammate = gEngfuncs.pfnGetCvarPointer("cl_forceteammate_enable");
	bool bTeammate = pCvarTeammate ? (pCvarTeammate->value != 0.0f) : true;
	createCheckbox("Force Teammate Models", bTeammate, m_pForceTeammateCheckbox);

	cvar_t* pCvarEnemy = gEngfuncs.pfnGetCvarPointer("cl_forceenemy_enable");
	bool bEnemy = pCvarEnemy ? (pCvarEnemy->value != 0.0f) : true;
	createCheckbox("Force Enemy Models", bEnemy, m_pForceEnemyCheckbox);

	cvar_t* pCvarExplosions = gEngfuncs.pfnGetCvarPointer("cl_explosions_enable");
	bool bExplosions = pCvarExplosions ? (pCvarExplosions->value != 0.0f) : true;
	createCheckbox("Enable Explosion Effects", bExplosions, m_pExplosionsCheckbox);

	cvar_t* pCvarDiscord = gEngfuncs.pfnGetCvarPointer("cl_discord_rpc");
	bool bDiscord = pCvarDiscord ? (pCvarDiscord->value != 0.0f) : true;
	createCheckbox("Discord Rich Presence", bDiscord, m_pDiscordRpcCheckbox);

	cvar_t* pCvarTimers = gEngfuncs.pfnGetCvarPointer("cl_item_timers");
	bool bTimers = pCvarTimers ? (pCvarTimers->value != 0.0f) : true;
	createCheckbox("Item Spawn Timers", bTimers, m_pItemTimersCheckbox);

	cvar_t* pCvarDamage = gEngfuncs.pfnGetCvarPointer("cl_damage_numbers");
	bool bDamage = pCvarDamage ? (pCvarDamage->value != 0.0f) : true;
	createCheckbox("Show Damage Numbers", bDamage, m_pDamageNumbersCheckbox);

	// Slider for Footstep Volume (0% to 200%)
	cvar_t* pCvarFootstep = gEngfuncs.pfnGetCvarPointer("cl_footstep_volume");
	int curFootstep = pCvarFootstep ? (int)(pCvarFootstep->value * 100.0f) : 100;

	m_pFootstepVolLabel = new vgui::Label("", ctrlX, curY);
	m_pFootstepVolLabel->setParent(this);
	if (pTextFont) {
		m_pFootstepVolLabel->setFont(pTextFont);
		m_pFootstepVolLabel->setFgColor(255, 255, 255, 0);
	}
	m_pFootstepVolLabel->setBgColor(0, 0, 0, 255);

	m_pFootstepVolSlider = new vgui::Slider2(ctrlX, curY + YRES(12), ctrlW, YRES(12), false);
	m_pFootstepVolSlider->setParent(this);
	m_pFootstepVolSlider->setRange(0, 200);
	m_pFootstepVolSlider->setValue(curFootstep);
	m_pFootstepVolSlider->addIntChangeSignal(this);

	UpdateLabelText(m_pFootstepVolLabel, "Footstep Sound Volume", curFootstep);
	curY += YRES(35);

	// Close Button
	m_pCloseButton = new CommandButton("Close", ctrlX, curY, ctrlW, YRES(24));
	m_pCloseButton->setParent(this);
	m_pCloseButton->addActionSignal(new CMenuHandler_StringCommand("", true));
	if (pTextFont)
		m_pCloseButton->setFont(pTextFont);
}

void CAGSettingsPanel::Open(void)
{
	CMenuPanel::Open();

	// Update checkbox values from cvars on open
	auto updateCheckbox = [](cvar_t* pCvar, vgui::CCheckButton2* pCheckbox, bool defaultVal) {
		if (pCvar && pCheckbox)
			pCheckbox->SetChecked(pCvar->value != 0.0f);
		else if (pCheckbox)
			pCheckbox->SetChecked(defaultVal);
	};

	updateCheckbox(gEngfuncs.pfnGetCvarPointer("cl_forceteammate_enable"), m_pForceTeammateCheckbox, true);
	updateCheckbox(gEngfuncs.pfnGetCvarPointer("cl_forceenemy_enable"), m_pForceEnemyCheckbox, true);
	updateCheckbox(gEngfuncs.pfnGetCvarPointer("cl_explosions_enable"), m_pExplosionsCheckbox, true);
	updateCheckbox(gEngfuncs.pfnGetCvarPointer("cl_discord_rpc"), m_pDiscordRpcCheckbox, true);
	updateCheckbox(gEngfuncs.pfnGetCvarPointer("cl_item_timers"), m_pItemTimersCheckbox, true);
	updateCheckbox(gEngfuncs.pfnGetCvarPointer("cl_damage_numbers"), m_pDamageNumbersCheckbox, true);

	// Update slider
	cvar_t* pCvarFootstep = gEngfuncs.pfnGetCvarPointer("cl_footstep_volume");
	int curFootstep = pCvarFootstep ? (int)(pCvarFootstep->value * 100.0f) : 100;
	if (m_pFootstepVolSlider)
		m_pFootstepVolSlider->setValue(curFootstep);
	UpdateLabelText(m_pFootstepVolLabel, "Footstep Sound Volume", curFootstep);
}

void CAGSettingsPanel::Close(void)
{
	CMenuPanel::Close();
	gViewPort->UpdateCursorState();
}

void CAGSettingsPanel::UpdateLabelText(vgui::Label* label, const char* name, int value)
{
	if (!label) return;
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%s: %d%%", name, value);
	label->setText(buf);
}

void CAGSettingsPanel::intChanged(int value, vgui::Panel* panel)
{
	if (panel == m_pFootstepVolSlider) {
		gEngfuncs.Cvar_SetValue("cl_footstep_volume", (float)value / 100.0f);
		UpdateLabelText(m_pFootstepVolLabel, "Footstep Sound Volume", value);
	}
}

void CAGSettingsPanel::StateChanged(vgui::CCheckButton2* pButton)
{
	if (pButton == m_pForceTeammateCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_forceteammate_enable", pButton->IsChecked() ? 1.0f : 0.0f);
		// Force models cache needs to be updated
		gEngfuncs.pfnClientCmd("cl_forceteammodel_list\n"); // trigger reload/refresh
	}
	else if (pButton == m_pForceEnemyCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_forceenemy_enable", pButton->IsChecked() ? 1.0f : 0.0f);
		gEngfuncs.pfnClientCmd("cl_forcemodel_list\n"); // trigger reload/refresh
	}
	else if (pButton == m_pExplosionsCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_explosions_enable", pButton->IsChecked() ? 1.0f : 0.0f);
	}
	else if (pButton == m_pDiscordRpcCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_discord_rpc", pButton->IsChecked() ? 1.0f : 0.0f);
	}
	else if (pButton == m_pItemTimersCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_item_timers", pButton->IsChecked() ? 1.0f : 0.0f);
	}
	else if (pButton == m_pDamageNumbersCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_damage_numbers", pButton->IsChecked() ? 1.0f : 0.0f);
	}
}
