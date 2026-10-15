//
// vgui_CookiesWindow.cpp
//
// Cookie consent wall shown by the "Accept All Cookies" instant mutator.
// Accept dismisses the wall; Reject disconnects the player from the server.
//

#include "VGUI_Font.h"
#include "VGUI_ScrollPanel.h"
#include "VGUI_TextImage.h"

#include "hud.h"
#include "cl_util.h"
#include "const.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"

#define COOKIES_WINDOW_X			XRES(80)
#define COOKIES_WINDOW_Y			YRES(40)
#define COOKIES_WINDOW_SIZE_X		XRES(480)
#define COOKIES_WINDOW_SIZE_Y		YRES(400)
#define COOKIES_MARGIN_X			XRES(16)
#define COOKIES_MARGIN_Y			YRES(16)
#define COOKIES_SCROLL_Y			YRES(48)
#define COOKIES_BUTTON_SIZE_X		XRES(200)
#define COOKIES_BODY_MAX			8192
#define COOKIES_INPUT_DELAY			0.5f

// CTransparentPanel fills _size from the border-inset origin, which paints over the right/bottom border lines.
class CCookiesWindowPanel : public Panel
{
public:
	CCookiesWindowPanel( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall ) {}

	virtual void paintBackground()
	{
		int wide, tall;
		getPaintSize( wide, tall );
		drawSetColor( 0, 0, 0, 30 );
		drawFilledRect( 0, 0, wide, tall );
	}
};

class CMenuHandler_CookieChoice : public ActionSignal
{
private:
	CCookiesPanel	*m_pPanel;
	bool			m_bReject;

public:
	CMenuHandler_CookieChoice( CCookiesPanel *pPanel, bool bReject )
	{
		m_pPanel = pPanel;
		m_bReject = bReject;
	}

	virtual void actionPerformed( Panel *panel )
	{
		m_pPanel->Choose( m_bReject );
	}
};

CCookiesPanel::CCookiesPanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	m_bAllowDisconnect = true;

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hBodyScheme = pSchemes->getSchemeHandle( "Briefing Text" );

	int r, g, b, a;

	CCookiesWindowPanel *pWindow = new CCookiesWindowPanel( COOKIES_WINDOW_X, COOKIES_WINDOW_Y, COOKIES_WINDOW_SIZE_X, COOKIES_WINDOW_SIZE_Y );
	pWindow->setParent( this );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	m_pWindowBorder = new LineBorder( Color( r, g, b, 0 ) );
	pWindow->setBorder( m_pWindowBorder );
	pWindow->setVisible( true );

	int iXPos, iYPos, iXSize, iYSize;
	pWindow->getPos( iXPos, iYPos );
	pWindow->getSize( iXSize, iYSize );

	Label *pTitle = new Label( "", iXPos + COOKIES_MARGIN_X, iYPos + COOKIES_MARGIN_Y );
	pTitle->setParent( this );
	pTitle->setFont( pSchemes->getFont( hTitleScheme ) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pTitle->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pTitle->setBgColor( r, g, b, a );
	pTitle->setContentAlignment( vgui::Label::a_west );
	pTitle->setText( "%s", CHudTextMessage::BufferedLocaliseTextString( "#Cookies_Title" ) );

	int iButtonY = iYPos + iYSize - COOKIES_MARGIN_Y - BUTTON_SIZE_Y;
	int iScrollY = iYPos + COOKIES_SCROLL_Y;

	m_pScrollPanel = new CTFScrollPanel( iXPos + COOKIES_MARGIN_X, iScrollY, iXSize - COOKIES_MARGIN_X * 2, iButtonY - COOKIES_MARGIN_Y - iScrollY );
	m_pScrollPanel->setParent( this );

	// Force the scrollbars on so the client clip accounts for them while wrapping.
	m_pScrollPanel->setScrollBarAutoVisible( false, false );
	m_pScrollPanel->setScrollBarVisible( true, true );
	m_pScrollPanel->validate();

	// titles.txt is CRLF and the agreement spans many lines; drop the CRs before wrapping.
	char szBody[COOKIES_BODY_MAX];
	const char *pszAgreement = gHUD.m_TextMessage.LookupString( "#Cookies_Agreement" );
	int len = 0;
	for ( const char *src = pszAgreement; *src && len < COOKIES_BODY_MAX - 1; src++ )
	{
		if ( *src != '\r' )
			szBody[len++] = *src;
	}
	szBody[len] = 0;

	TextPanel *pText = new TextPanel( "", 0, 0, 64, 64 );
	pText->setParent( m_pScrollPanel->getClient() );
	pText->setFont( pSchemes->getFont( hBodyScheme ) );
	pSchemes->getFgColor( hBodyScheme, r, g, b, a );
	pText->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hBodyScheme, r, g, b, a );
	pText->setBgColor( r, g, b, a );
	pText->setText( szBody );
	pText->addInputSignal( new CHandler_MenuWheelForward( this ) );

	int iScrollSizeX, iScrollSizeY;
	pText->getTextImage()->setSize( m_pScrollPanel->getClientClip()->getWide(), m_pScrollPanel->getClientClip()->getTall() );
	pText->getTextImage()->getTextSizeWrapped( iScrollSizeX, iScrollSizeY );
	pText->setSize( iScrollSizeX, iScrollSizeY );

	m_pScrollPanel->setScrollBarAutoVisible( true, true );
	m_pScrollPanel->setScrollBarVisible( false, false );
	m_pScrollPanel->validate();
	m_pScrollPanel->addInputSignal( new CHandler_MenuWheelForward( this ) );
	m_pScrollPanel->getClient()->addInputSignal( new CHandler_MenuWheelForward( this ) );

	pSchemes->getFgColor( hTitleScheme, r, g, b, a );

	ColorButton *pAccept = new ColorButton( CHudTextMessage::BufferedLocaliseTextString( "#Cookies_Accept" ), iXPos + COOKIES_MARGIN_X, iButtonY, COOKIES_BUTTON_SIZE_X, BUTTON_SIZE_Y, false, true );
	pAccept->setBoundKey( (char)255 );
	pAccept->setContentAlignment( vgui::Label::a_center );
	pAccept->setUnArmedBorderColor( r, g, b, 0 );
	pAccept->setArmedBorderColor( r, g, b, 0 );
	pAccept->addActionSignal( new CMenuHandler_CookieChoice( this, false ) );
	pAccept->setParent( this );

	// Real cookie walls bury the reject option; keep it a little fainter.
	ColorButton *pReject = new ColorButton( CHudTextMessage::BufferedLocaliseTextString( "#Cookies_Reject" ), iXPos + iXSize - COOKIES_MARGIN_X - COOKIES_BUTTON_SIZE_X, iButtonY, COOKIES_BUTTON_SIZE_X, BUTTON_SIZE_Y, false, true );
	pReject->setBoundKey( (char)255 );
	pReject->setContentAlignment( vgui::Label::a_center );
	pReject->setUnArmedBorderColor( r, g, b, 160 );
	pReject->setArmedBorderColor( r, g, b, 0 );
	pReject->addActionSignal( new CMenuHandler_CookieChoice( this, true ) );
	pReject->setParent( this );
}

void CCookiesPanel::Open( void )
{
	int r, g, b;
	UnpackRGB( r, g, b, HudColor() );
	m_pWindowBorder->setLineColor( r, g, b, 0 );

	m_pScrollPanel->setScrollValue( 0, 0 );

	PlaySound( "acceptall.wav", 1 );

	CMenuPanel::Open();
}

void CCookiesPanel::Choose( bool bReject )
{
	// Ignore clicks carried over from firing when the wall popped up.
	if ( ( gHUD.m_flTime - GetOpenTime() ) < COOKIES_INPUT_DELAY )
		return;

	PlaySound( "wpn_select.wav", 1 );

	if ( gViewPort->m_pCurrentMenu == this )
		gViewPort->HideTopMenu();

	if ( bReject && m_bAllowDisconnect )
		gEngfuncs.pfnClientCmd( "disconnect\n" );
}
