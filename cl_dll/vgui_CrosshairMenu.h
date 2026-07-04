#pragma once

#include "vgui_TeamFortressViewport.h"
#include "vgui_slider2.h"
#include "vgui_checkbutton2.h"
#include <VGUI_Label.h>
#include <VGUI_IntChangeSignal.h>

class CCrosshairMenuPanel : public CMenuPanel, public vgui::IntChangeSignal, public vgui::ICheckButton2Handler
{
private:
	vgui::CCheckButton2* m_pEnableCheckbox;
	vgui::Label* m_pOutlineLabel;
	vgui::Slider2* m_pOutlineSlider;

	vgui::Label* m_pSizeLabel;
	vgui::Slider2* m_pSizeSlider;

	vgui::Label* m_pThicknessLabel;
	vgui::Slider2* m_pThicknessSlider;

	vgui::Label* m_pGapLabel;
	vgui::Slider2* m_pGapSlider;

	vgui::Label* m_pAlphaLabel;
	vgui::Slider2* m_pAlphaSlider;

	vgui::Label* m_pColorRLabel;
	vgui::Slider2* m_pColorRSlider;

	vgui::Label* m_pColorGLabel;
	vgui::Slider2* m_pColorGSlider;

	vgui::Label* m_pColorBLabel;
	vgui::Slider2* m_pColorBSlider;

	vgui::Label* m_pCircleRadiusLabel;
	vgui::Slider2* m_pCircleRadiusSlider;

	vgui::Label* m_pDotSizeLabel;
	vgui::Slider2* m_pDotSizeSlider;

	vgui::CCheckButton2* m_pTopLineCheckbox;
	vgui::CCheckButton2* m_pBottomLineCheckbox;
	vgui::CCheckButton2* m_pLeftLineCheckbox;
	vgui::CCheckButton2* m_pRightLineCheckbox;

	CommandButton* m_pCloseButton;

	void UpdateLabelText(vgui::Label* label, const char* name, int value);

public:
	CCrosshairMenuPanel(int iTrans, int iRemoveMe, int x, int y, int wide, int tall);

	virtual void Open(void) override;
	virtual void Close(void) override;
	virtual void intChanged(int value, vgui::Panel* panel) override;
	virtual void StateChanged(vgui::CCheckButton2* pButton) override;
};
