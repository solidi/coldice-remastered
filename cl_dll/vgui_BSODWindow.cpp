//
// vgui_BSODWindow.cpp
//
// Fake crash screen shown by the "bsod" instant mutator.
// The "Failed Successfully" button dismisses it.
//

#include "VGUI_Font.h"
#include "VGUI_TextImage.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"

#define BSOD_MARGIN_X			XRES(48)
#define BSOD_HEADER_Y			YRES(40)
#define BSOD_HEADER_PAD_X		XRES(12)
#define BSOD_HEADER_PAD_Y		YRES(4)
#define BSOD_BODY_GAP_Y			YRES(24)
#define BSOD_BUTTON_SIZE_X		XRES(160)
#define BSOD_BUTTON_MARGIN_Y	YRES(32)
#define BSOD_BODY_MAX			4096
#define BSOD_INPUT_DELAY		0.5f

#define BSOD_BLUE_R				0
#define BSOD_BLUE_G				0
#define BSOD_BLUE_B				170
#define BSOD_GRAY				170

class CMenuHandler_BSODDismiss : public ActionSignal
{
private:
	CBSODPanel	*m_pPanel;

public:
	CMenuHandler_BSODDismiss( CBSODPanel *pPanel )
	{
		m_pPanel = pPanel;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pPanel->Dismiss();
	}
};

CBSODPanel::CBSODPanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hBodyScheme = pSchemes->getSchemeHandle( "Briefing Text" );

	// Classic gray title block with blue text, centred near the top.
	Label *pHeader = new Label( "", 0, 0 );
	pHeader->setParent( this );
	pHeader->setFont( pSchemes->getFont( hTitleScheme ) );
	pHeader->setText( "%s", CHudTextMessage::BufferedLocaliseTextString( "#BSOD_Title" ) );
	pHeader->setFgColor( BSOD_BLUE_R, BSOD_BLUE_G, BSOD_BLUE_B, 0 );
	pHeader->setBgColor( BSOD_GRAY, BSOD_GRAY, BSOD_GRAY, 0 );

	int iHeaderWide, iHeaderTall;
	pHeader->getTextSize( iHeaderWide, iHeaderTall );
	iHeaderWide += BSOD_HEADER_PAD_X * 2;
	iHeaderTall += BSOD_HEADER_PAD_Y * 2;
	pHeader->setContentFitted( false );
	pHeader->setSize( iHeaderWide, iHeaderTall );
	pHeader->setPos( ( wide - iHeaderWide ) / 2, BSOD_HEADER_Y );
	pHeader->setContentAlignment( vgui::Label::a_center );

	int iButtonY = tall - BSOD_BUTTON_MARGIN_Y - BUTTON_SIZE_Y;
	int iBodyY = BSOD_HEADER_Y + iHeaderTall + BSOD_BODY_GAP_Y;

	// titles.txt is CRLF; drop the CRs so VGUI does not draw them.
	char szBody[BSOD_BODY_MAX];
	const char *pszBody = gHUD.m_TextMessage.LookupString( "#BSOD_Body" );
	int len = 0;
	for ( const char *src = pszBody; *src && len < BSOD_BODY_MAX - 1; src++ )
	{
		if ( *src != '\r' )
			szBody[len++] = *src;
	}
	szBody[len] = 0;

	int iBodyWide = wide - BSOD_MARGIN_X * 2;
	int iBodyTall = iButtonY - BSOD_BODY_GAP_Y - iBodyY;

	TextPanel *pText = new TextPanel( "", BSOD_MARGIN_X, iBodyY, iBodyWide, iBodyTall );
	pText->setParent( this );
	pText->setFont( pSchemes->getFont( hBodyScheme ) );
	pText->setFgColor( 255, 255, 255, 0 );
	pText->setBgColor( 0, 0, 0, 255 );
	pText->setText( szBody );
	pText->getTextImage()->setSize( iBodyWide, iBodyTall );

	ColorButton *pButton = new ColorButton( CHudTextMessage::BufferedLocaliseTextString( "#BSOD_Button" ), wide - BSOD_MARGIN_X - BSOD_BUTTON_SIZE_X, iButtonY, BSOD_BUTTON_SIZE_X, BUTTON_SIZE_Y, false, true );
	pButton->setBoundKey( (char)255 );
	pButton->setContentAlignment( vgui::Label::a_center );
	pButton->setUnArmedColor( 255, 255, 255, 0 );
	pButton->setArmedColor( BSOD_BLUE_R, BSOD_BLUE_G, BSOD_BLUE_B, 0 );
	pButton->setBgColor( 255, 255, 255, 0 );
	pButton->setUnArmedBorderColor( 255, 255, 255, 0 );
	pButton->setArmedBorderColor( 255, 255, 255, 0 );
	pButton->addActionSignal( new CMenuHandler_BSODDismiss( this ) );
	pButton->setParent( this );
}

void CBSODPanel::paintBackground( void )
{
	drawSetColor( BSOD_BLUE_R, BSOD_BLUE_G, BSOD_BLUE_B, 0 );
	drawFilledRect( 0, 0, _size[0], _size[1] );
}

void CBSODPanel::Open( void )
{
	PlaySound( "bsod.wav", 1 );

	CMenuPanel::Open();
}

void CBSODPanel::Dismiss( void )
{
	// Ignore clicks carried over from firing when the screen popped up.
	if ( ( gHUD.m_flTime - GetOpenTime() ) < BSOD_INPUT_DELAY )
		return;

	PlaySound( "wpn_select.wav", 1 );

	if ( gViewPort->m_pCurrentMenu == this )
		gViewPort->HideTopMenu();
}
