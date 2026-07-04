#include "hud.h"
#include "cl_util.h"
#undef min
#undef max
#include "vgui_CrosshairMenu.h"
#include "vgui_int.h"
#include <VGUI_Font.h>
#include <VGUI_ActionSignal.h>
#include <VGUI_IntChangeSignal.h>
#include <cstdio>
#include <cstring>

using namespace vgui;

CCrosshairMenuPanel::CCrosshairMenuPanel(int iTrans, int iRemoveMe, int x, int y, int wide, int tall)
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
	vgui::Label* pTitle = new vgui::Label("CROSSHAIR EDITOR", XRES(15), YRES(10), XRES(230), YRES(20));
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
	auto createCheckbox = [&](const char* text, bool checked, vgui::CCheckButton2*& member, int cx, int cy, int cw) {
		member = new vgui::CCheckButton2();
		member->setParent(this);
		member->SetImages("gfx/vgui/checked.tga", "gfx/vgui/unchecked.tga");
		member->SetText(text);
		member->SetChecked(checked);
		member->SetHandler(this);
		member->setPos(cx, cy);
		if (pTextFont)
			member->SetTextColor(255, 255, 255, 0); // white foreground
	};

	// Slider layout helper
	auto createSlider = [&](const char* labelText, int minVal, int maxVal, int curVal, vgui::Label*& labelMember, vgui::Slider2*& sliderMember) {
		labelMember = new vgui::Label("", ctrlX, curY);
		labelMember->setParent(this);
		if (pTextFont) {
			labelMember->setFont(pTextFont);
			labelMember->setFgColor(255, 255, 255, 0);
		}
		labelMember->setBgColor(0, 0, 0, 255);

		sliderMember = new vgui::Slider2(ctrlX, curY + YRES(12), ctrlW, YRES(12), false);
		sliderMember->setParent(this);
		sliderMember->setRange(minVal, maxVal);
		sliderMember->setValue(curVal);
		sliderMember->addIntChangeSignal(this);

		UpdateLabelText(labelMember, labelText, curVal);
		curY += YRES(28);
	};

	// Retrieve CVar initial states
	cvar_t* pCvarCross = gEngfuncs.pfnGetCvarPointer("cl_cross");
	bool bEnabled = pCvarCross ? (pCvarCross->value != 0.0f) : false;

	createCheckbox("Enable Crosshair", bEnabled, m_pEnableCheckbox, ctrlX, curY, ctrlW);
	curY += YRES(22);

	cvar_t* pCvarSize = gEngfuncs.pfnGetCvarPointer("cl_cross_size");
	createSlider("Size", 0, 100, pCvarSize ? (int)pCvarSize->value : 10, m_pSizeLabel, m_pSizeSlider);

	cvar_t* pCvarThickness = gEngfuncs.pfnGetCvarPointer("cl_cross_thickness");
	createSlider("Thickness", 1, 20, pCvarThickness ? (int)pCvarThickness->value : 2, m_pThicknessLabel, m_pThicknessSlider);

	cvar_t* pCvarGap = gEngfuncs.pfnGetCvarPointer("cl_cross_gap");
	createSlider("Gap", -20, 50, pCvarGap ? (int)pCvarGap->value : 3, m_pGapLabel, m_pGapSlider);

	cvar_t* pCvarOutline = gEngfuncs.pfnGetCvarPointer("cl_cross_outline");
	createSlider("Outline", 0, 10, pCvarOutline ? (int)pCvarOutline->value : 0, m_pOutlineLabel, m_pOutlineSlider);

	cvar_t* pCvarDotSize = gEngfuncs.pfnGetCvarPointer("cl_cross_dot_size");
	createSlider("Dot Size", 0, 50, pCvarDotSize ? (int)pCvarDotSize->value : 0, m_pDotSizeLabel, m_pDotSizeSlider);

	cvar_t* pCvarCircleRadius = gEngfuncs.pfnGetCvarPointer("cl_cross_circle_radius");
	createSlider("Circle Radius", 0, 100, pCvarCircleRadius ? (int)pCvarCircleRadius->value : 0, m_pCircleRadiusLabel, m_pCircleRadiusSlider);

	cvar_t* pCvarAlpha = gEngfuncs.pfnGetCvarPointer("cl_cross_alpha");
	createSlider("Alpha", 0, 255, pCvarAlpha ? (int)pCvarAlpha->value : 200, m_pAlphaLabel, m_pAlphaSlider);

	// Colors parsing
	int initialR = 0, initialG = 255, initialB = 0;
	cvar_t* pCvarColor = gEngfuncs.pfnGetCvarPointer("cl_cross_color");
	if (pCvarColor && pCvarColor->string)
	{
		std::sscanf(pCvarColor->string, "%d %d %d", &initialR, &initialG, &initialB);
	}
	createSlider("Color R", 0, 255, initialR, m_pColorRLabel, m_pColorRSlider);
	createSlider("Color G", 0, 255, initialG, m_pColorGLabel, m_pColorGSlider);
	createSlider("Color B", 0, 255, initialB, m_pColorBLabel, m_pColorBSlider);

	// Line checkboxes (in 2x2 grid)
	cvar_t* pTop = gEngfuncs.pfnGetCvarPointer("cl_cross_top_line");
	cvar_t* pBottom = gEngfuncs.pfnGetCvarPointer("cl_cross_bottom_line");
	cvar_t* pLeft = gEngfuncs.pfnGetCvarPointer("cl_cross_left_line");
	cvar_t* pRight = gEngfuncs.pfnGetCvarPointer("cl_cross_right_line");

	int cx1 = XRES(15);
	int cx2 = XRES(135);
	int cy1 = curY;
	int cy2 = curY + YRES(18);

	createCheckbox("Top Line", pTop ? (pTop->value != 0.0f) : true, m_pTopLineCheckbox, cx1, cy1, XRES(110));
	createCheckbox("Bottom Line", pBottom ? (pBottom->value != 0.0f) : true, m_pBottomLineCheckbox, cx2, cy1, XRES(110));
	createCheckbox("Left Line", pLeft ? (pLeft->value != 0.0f) : true, m_pLeftLineCheckbox, cx1, cy2, XRES(110));
	createCheckbox("Right Line", pRight ? (pRight->value != 0.0f) : true, m_pRightLineCheckbox, cx2, cy2, XRES(110));
	curY += YRES(38);

	// Close Button
	m_pCloseButton = new CommandButton("Close", XRES(15), curY, XRES(230), YRES(24));
	m_pCloseButton->setParent(this);
	if (pTextFont)
		m_pCloseButton->setFont(pTextFont);
	m_pCloseButton->addActionSignal(new CMenuHandler_StringCommand("", true));
}

void CCrosshairMenuPanel::Open(void)
{
	CMenuPanel::Open();

	// Enable checkbox
	cvar_t* pCvarCross = gEngfuncs.pfnGetCvarPointer("cl_cross");
	if (m_pEnableCheckbox && pCvarCross)
		m_pEnableCheckbox->SetChecked(pCvarCross->value != 0.0f);

	// Sliders
	auto updateSlider = [&](cvar_t* pCvar, vgui::Slider2* pSlider, vgui::Label* pLabel, const char* name) {
		if (pCvar && pSlider) {
			pSlider->setValue((int)pCvar->value);
			UpdateLabelText(pLabel, name, (int)pCvar->value);
		}
	};

	updateSlider(gEngfuncs.pfnGetCvarPointer("cl_cross_size"), m_pSizeSlider, m_pSizeLabel, "Size");
	updateSlider(gEngfuncs.pfnGetCvarPointer("cl_cross_thickness"), m_pThicknessSlider, m_pThicknessLabel, "Thickness");
	updateSlider(gEngfuncs.pfnGetCvarPointer("cl_cross_gap"), m_pGapSlider, m_pGapLabel, "Gap");
	updateSlider(gEngfuncs.pfnGetCvarPointer("cl_cross_outline"), m_pOutlineSlider, m_pOutlineLabel, "Outline");
	updateSlider(gEngfuncs.pfnGetCvarPointer("cl_cross_dot_size"), m_pDotSizeSlider, m_pDotSizeLabel, "Dot Size");
	updateSlider(gEngfuncs.pfnGetCvarPointer("cl_cross_circle_radius"), m_pCircleRadiusSlider, m_pCircleRadiusLabel, "Circle Radius");
	updateSlider(gEngfuncs.pfnGetCvarPointer("cl_cross_alpha"), m_pAlphaSlider, m_pAlphaLabel, "Alpha");

	// Colors
	cvar_t* pCvarColor = gEngfuncs.pfnGetCvarPointer("cl_cross_color");
	if (pCvarColor && pCvarColor->string && m_pColorRSlider && m_pColorGSlider && m_pColorBSlider)
	{
		int r = 0, g = 255, b = 0;
		if (std::sscanf(pCvarColor->string, "%d %d %d", &r, &g, &b) == 3)
		{
			m_pColorRSlider->setValue(r);
			m_pColorGSlider->setValue(g);
			m_pColorBSlider->setValue(b);
			UpdateLabelText(m_pColorRLabel, "Color R", r);
			UpdateLabelText(m_pColorGLabel, "Color G", g);
			UpdateLabelText(m_pColorBLabel, "Color B", b);
		}
	}

	// Line checkboxes
	auto updateLine = [&](cvar_t* pCvar, vgui::CCheckButton2* pCheckbox) {
		if (pCvar && pCheckbox)
			pCheckbox->SetChecked(pCvar->value != 0.0f);
	};

	updateLine(gEngfuncs.pfnGetCvarPointer("cl_cross_top_line"), m_pTopLineCheckbox);
	updateLine(gEngfuncs.pfnGetCvarPointer("cl_cross_bottom_line"), m_pBottomLineCheckbox);
	updateLine(gEngfuncs.pfnGetCvarPointer("cl_cross_left_line"), m_pLeftLineCheckbox);
	updateLine(gEngfuncs.pfnGetCvarPointer("cl_cross_right_line"), m_pRightLineCheckbox);
}

void CCrosshairMenuPanel::Close(void)
{
	CMenuPanel::Close();
	gViewPort->UpdateCursorState();
}

void CCrosshairMenuPanel::UpdateLabelText(vgui::Label* label, const char* name, int value)
{
	if (!label) return;
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%s: %d", name, value);
	label->setText(buf);
}

void CCrosshairMenuPanel::intChanged(int value, vgui::Panel* panel)
{
	if (panel == m_pSizeSlider) {
		gEngfuncs.Cvar_SetValue("cl_cross_size", value);
		UpdateLabelText(m_pSizeLabel, "Size", value);
	}
	else if (panel == m_pThicknessSlider) {
		gEngfuncs.Cvar_SetValue("cl_cross_thickness", value);
		UpdateLabelText(m_pThicknessLabel, "Thickness", value);
	}
	else if (panel == m_pGapSlider) {
		gEngfuncs.Cvar_SetValue("cl_cross_gap", value);
		UpdateLabelText(m_pGapLabel, "Gap", value);
	}
	else if (panel == m_pOutlineSlider) {
		gEngfuncs.Cvar_SetValue("cl_cross_outline", value);
		UpdateLabelText(m_pOutlineLabel, "Outline", value);
	}
	else if (panel == m_pDotSizeSlider) {
		gEngfuncs.Cvar_SetValue("cl_cross_dot_size", value);
		UpdateLabelText(m_pDotSizeLabel, "Dot Size", value);
	}
	else if (panel == m_pCircleRadiusSlider) {
		gEngfuncs.Cvar_SetValue("cl_cross_circle_radius", value);
		UpdateLabelText(m_pCircleRadiusLabel, "Circle Radius", value);
	}
	else if (panel == m_pAlphaSlider) {
		gEngfuncs.Cvar_SetValue("cl_cross_alpha", value);
		UpdateLabelText(m_pAlphaLabel, "Alpha", value);
	}
	else if (panel == m_pColorRSlider || panel == m_pColorGSlider || panel == m_pColorBSlider) {
		int r = m_pColorRSlider->getValue();
		int g = m_pColorGSlider->getValue();
		int b = m_pColorBSlider->getValue();

		if (panel == m_pColorRSlider) UpdateLabelText(m_pColorRLabel, "Color R", value);
		else if (panel == m_pColorGSlider) UpdateLabelText(m_pColorGLabel, "Color G", value);
		else if (panel == m_pColorBSlider) UpdateLabelText(m_pColorBLabel, "Color B", value);

		char cmd[128];
		std::snprintf(cmd, sizeof(cmd), "cl_cross_color \"%d %d %d\"\n", r, g, b);
		gEngfuncs.pfnClientCmd(cmd);
	}
}

void CCrosshairMenuPanel::StateChanged(vgui::CCheckButton2* pButton)
{
	if (pButton == m_pEnableCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_cross", pButton->IsChecked() ? 1.0f : 0.0f);
	}
	else if (pButton == m_pTopLineCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_cross_top_line", pButton->IsChecked() ? 1.0f : 0.0f);
	}
	else if (pButton == m_pBottomLineCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_cross_bottom_line", pButton->IsChecked() ? 1.0f : 0.0f);
	}
	else if (pButton == m_pLeftLineCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_cross_left_line", pButton->IsChecked() ? 1.0f : 0.0f);
	}
	else if (pButton == m_pRightLineCheckbox) {
		gEngfuncs.Cvar_SetValue("cl_cross_right_line", pButton->IsChecked() ? 1.0f : 0.0f);
	}
}
