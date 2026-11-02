/*
==============
vgui_VoteBotOptionsWindow.cpp

Implements CVoteBotOptionsPanel -- the RTV-only bot-options vote panel.
Rows come from the server's botoptions.txt (see workspace/ai/bot_options_system.md).
Layout and tally display match CVoteServerOptionsPanel.
==============
*/

#include "VGUI_Font.h"
#include "VGUI_ScrollPanel.h"
#include "VGUI_ScrollBar.h"
#include <VGUI_TextImage.h>
#include <VGUI_Scheme.h>
#include <VGUI_App.h>

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_ServerBrowser.h"

#define BOMENU_TITLE_X              XRES(80)
#define BOMENU_TITLE_Y              YRES(32)
#define BOMENU_SCROLL_X             XRES(80)
#define BOMENU_SCROLL_Y             YRES(80)
#define BOMENU_SCROLL_WIDE          XRES(480)
#define BOMENU_SCROLL_TALL          YRES(340)
#define BOMENU_ROW_HEIGHT           YRES(36)
#define BOMENU_ROW_SPACER_Y         YRES(4)
#define BOMENU_LABEL_WIDE           XRES(140)
#define BOMENU_BUTTON_WIDE          XRES(80)
#define BOMENU_BUTTON_GAP           XRES(4)

// The manifest is usable once it fully arrived with the revision the vote was opened with.
static bool BotOptionsManifestReady( void )
{
	return g_bBotOptionsReceived
		&& g_iBotOptionsRevisionClient == g_iBotOptionsVoteRevisionClient
		&& g_iActiveBotOptionsClientCount > 0;
}

CVoteBotOptionsPanel::CVoteBotOptionsPanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall )
	: CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );

	int r, g, b, a;

	m_pTitleLabel = new Label( "", BOMENU_TITLE_X, BOMENU_TITLE_Y );
	m_pTitleLabel->setParent( this );
	m_pTitleLabel->setFont( pSchemes->getFont( hTitleScheme ) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	m_pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	m_pTitleLabel->setBgColor( r, g, b, a );
	m_pTitleLabel->setContentAlignment( vgui::Label::a_west );
	m_pTitleLabel->setText( "Bot Options for ?" );

	m_pScrollPanel = new CTFScrollPanel( BOMENU_SCROLL_X, BOMENU_SCROLL_Y, BOMENU_SCROLL_WIDE, BOMENU_SCROLL_TALL );
	m_pScrollPanel->setParent( this );
	m_pScrollPanel->setScrollBarAutoVisible( true, true );
	m_pScrollPanel->setScrollBarVisible( false, true );
	m_pScrollPanelBorder = new LineBorder( Color( r, g, b, 255 ) );
	m_pScrollPanel->setBorder( m_pScrollPanelBorder );
	m_pScrollPanel->validate();
	m_pScrollPanel->addInputSignal( new CHandler_MenuWheelForward(this) );
	m_pScrollPanel->getClient()->addInputSignal( new CHandler_MenuWheelForward(this) );

	m_iRowCount = 0;
	m_bWaitingForManifest = true;
	m_fAutoCloseTime = 0;
	for ( int k = 0; k < MAX_CLIENT_BOT_OPTIONS; k++ )
	{
		m_pRowLabels[k] = NULL;
		for ( int o = 0; o < MAX_CLIENT_SERVER_OPTION_VALUES; o++ )
		{
			m_pRowButtons[k][o] = NULL;
			m_pRowButtonBorders[k][o] = NULL;
			m_pRowVoteTallies[k][o] = NULL;
		}
	}
}

void CVoteBotOptionsPanel::BuildRows( void )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );
	SchemeHandle_t hPrimaryButtonText = pSchemes->getSchemeHandle( "Primary Button Text" );

	int r, g, b, a;
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );

	for ( int k = 0; k < MAX_CLIENT_BOT_OPTIONS; k++ )
	{
		if ( m_pRowLabels[k] )
		{
			m_pRowLabels[k]->setVisible( false );
			m_pRowLabels[k] = NULL;
		}
		for ( int o = 0; o < MAX_CLIENT_SERVER_OPTION_VALUES; o++ )
		{
			if ( m_pRowButtons[k][o] )
			{
				m_pRowButtons[k][o]->setVisible( false );
				m_pRowButtons[k][o] = NULL;
			}
			m_pRowButtonBorders[k][o] = NULL;
			if ( m_pRowVoteTallies[k][o] )
			{
				m_pRowVoteTallies[k][o]->setVisible( false );
				m_pRowVoteTallies[k][o] = NULL;
			}
		}
	}

	m_iRowCount = g_iActiveBotOptionsClientCount;
	if ( m_iRowCount > MAX_CLIENT_BOT_OPTIONS )
		m_iRowCount = MAX_CLIENT_BOT_OPTIONS;

	m_bWaitingForManifest = !BotOptionsManifestReady();
	if ( m_bWaitingForManifest )
	{
		Label *pWait = new Label( "Waiting for bot-options manifest...",
			BOMENU_ROW_SPACER_Y, BOMENU_ROW_SPACER_Y );
		pWait->setParent( m_pScrollPanel->getClient() );
		pWait->setFont( pSchemes->getFont( hClassWindowText ) );
		pWait->setSize( BOMENU_SCROLL_WIDE - XRES(20), BOMENU_ROW_HEIGHT );
		pWait->setContentAlignment( vgui::Label::a_west );
		pWait->setFgColor( r, g, b, 0 );
		pWait->setBgColor( 0, 0, 0, 255 );
		pWait->addInputSignal( new CHandler_MenuWheelForward(this) );
		m_pRowLabels[0] = pWait;
		m_pScrollPanel->validate();
		return;
	}

	for ( int k = 0; k < m_iRowCount; k++ )
	{
		int realIdx = g_iActiveBotOptionsClient[k];
		if ( realIdx < 0 || realIdx >= g_iBotOptionsClientCount )
			continue;
		const client_bot_option_t &it = g_BotOptionsClient[realIdx];

		int yPos = BOMENU_ROW_SPACER_Y + k * ( BOMENU_ROW_HEIGHT + BOMENU_ROW_SPACER_Y );

		m_pRowLabels[k] = new Label( "", BOMENU_ROW_SPACER_Y, yPos );
		m_pRowLabels[k]->setParent( m_pScrollPanel->getClient() );
		m_pRowLabels[k]->setFont( pSchemes->getFont( ScreenWidth >= 1024 ? hTitleScheme : hPrimaryButtonText ) );
		m_pRowLabels[k]->setSize( BOMENU_LABEL_WIDE, BOMENU_ROW_HEIGHT );
		m_pRowLabels[k]->setContentAlignment( vgui::Label::a_west );
		m_pRowLabels[k]->setFgColor( r, g, b, 0 );
		m_pRowLabels[k]->setBgColor( 0, 0, 0, 255 );
		m_pRowLabels[k]->addInputSignal( new CHandler_MenuWheelForward(this) );

		char titleBuf[80];
		_snprintf( titleBuf, sizeof(titleBuf), " %s%s",
			it.title, it.restart ? " (restart)" : "" );
		titleBuf[sizeof(titleBuf) - 1] = 0;
		m_pRowLabels[k]->setText( titleBuf );

		int nopt = it.numOptions;
		if ( nopt > MAX_CLIENT_SERVER_OPTION_VALUES )
			nopt = MAX_CLIENT_SERVER_OPTION_VALUES;

		for ( int o = 0; o < nopt; o++ )
		{
			int xPos = BOMENU_ROW_SPACER_Y + BOMENU_LABEL_WIDE
			         + o * ( BOMENU_BUTTON_WIDE + BOMENU_BUTTON_GAP );

			char cmdBuf[28];
			_snprintf( cmdBuf, sizeof(cmdBuf), "votebotopt %d %d\n", k + 1, o + 1 );
			cmdBuf[sizeof(cmdBuf) - 1] = 0;
			ActionSignal *pASig = new CMenuHandler_StringCommand( cmdBuf, false );

			vgui::Font *font = pSchemes->getFont(hPrimaryButtonText);
			if (ScreenWidth <= 1024)
				font = pSchemes->getFont(hClassWindowText);

			char sz[64];
			_snprintf( sz, sizeof(sz), " %s", it.labels[o] );
			sz[sizeof(sz) - 1] = 0;
			m_pRowButtons[k][o] = new ColorButton( sz,
				xPos, yPos, BOMENU_BUTTON_WIDE, BOMENU_ROW_HEIGHT, false, true );
			m_pRowButtonBorders[k][o] = new LineBorder( Color(r, g, b, a) );
			m_pRowButtons[k][o]->setBorder( m_pRowButtonBorders[k][o] );
			m_pRowButtons[k][o]->setBoundKey( (char)255 );
			m_pRowButtons[k][o]->setContentAlignment( vgui::Label::a_west );
			m_pRowButtons[k][o]->addActionSignal( pASig );
			m_pRowButtons[k][o]->setParent( m_pScrollPanel->getClient() );
			m_pRowButtons[k][o]->setFont( font );

			m_pRowVoteTallies[k][o] = new Label( "0",
				BOMENU_BUTTON_WIDE - XRES(22), BOMENU_ROW_HEIGHT / 2.5f );
			m_pRowVoteTallies[k][o]->setParent( m_pRowButtons[k][o] );
			m_pRowVoteTallies[k][o]->setFont( font );
			m_pRowVoteTallies[k][o]->setContentAlignment( vgui::Label::a_northeast );
			m_pRowVoteTallies[k][o]->setFgColor( r, g, b, 0 );
			m_pRowVoteTallies[k][o]->setBgColor( 0, 0, 0, 255 );
			m_pRowVoteTallies[k][o]->addInputSignal( new CHandler_MenuWheelForward(this) );
		}
	}

	m_pScrollPanel->validate();
}

void CVoteBotOptionsPanel::Update()
{
	// The bot manifest streams after connect, so the vote can open before it
	// lands (or while a newer revision replaces it). Rebuild on either edge.
	if ( m_bWaitingForManifest == BotOptionsManifestReady() )
		BuildRows();

	float timeLeft = fmax( 0.0f, ( m_iTime + m_fStartTime - gHUD.m_flTime ) );

	int r, g, b, a = 0;
	UnpackRGB( r, g, b, HudColor() );

	if ( m_pScrollPanelBorder )
	{
		m_pScrollPanelBorder->setLineColor( r, g, b, 255 );
		m_pScrollPanel->setBorder( m_pScrollPanelBorder );
	}
	m_pTitleLabel->setFgColor( r, g, b, 0 );

	Scheme *pScheme = App::getInstance()->getScheme();
	pScheme->setColor( Scheme::sc_primary1,  r, g, b, a );
	pScheme->setColor( Scheme::sc_primary2,  r, g, b, a );
	pScheme->setColor( Scheme::sc_secondary1, r, g, b, a );

	ScrollBar *pVerticalScrollBar = m_pScrollPanel->getVerticalScrollBar();
	if ( pVerticalScrollBar )
		pVerticalScrollBar->repaint();

	int rowCount = m_bWaitingForManifest ? 0 : m_iRowCount;
	bool allVoted = ( g_bBotOptionsAutoCloseOnComplete && rowCount > 0 );
	for ( int k = 0; k < rowCount; k++ )
	{
		int realIdx = g_iActiveBotOptionsClient[k];
		if ( realIdx < 0 || realIdx >= g_iBotOptionsClientCount )
			continue;
		const client_bot_option_t &it = g_BotOptionsClient[realIdx];
		int nopt = it.numOptions;
		if ( nopt > MAX_CLIENT_SERVER_OPTION_VALUES )
			nopt = MAX_CLIENT_SERVER_OPTION_VALUES;

		int counts[MAX_CLIENT_SERVER_OPTION_VALUES] = { 0 };
		for ( int p = 1; p <= MAX_PLAYERS; p++ )
		{
			int v = g_PlayerBotOptVote[p][k];
			if ( v >= 0 && v < nopt )
				counts[v]++;
		}
		int hi = -1, hiCount = 0;
		for ( int o = 0; o < nopt; o++ )
		{
			if ( counts[o] > hiCount )
			{
				hiCount = counts[o];
				hi = o;
			}
		}

		int myPick = -1;
		for ( int p = 1; p <= MAX_PLAYERS; p++ )
		{
			if ( g_PlayerInfoList[p].thisplayer )
			{
				myPick = g_PlayerBotOptVote[p][k];
				break;
			}
		}
		if ( myPick < 0 )
			allVoted = false;

		if ( m_pRowLabels[k] )
		{
			char titleBuf[96];
			const char *marker = ( myPick >= 0 ) ? "[X]" : "[ ]";
			_snprintf( titleBuf, sizeof(titleBuf), " %s %s%s",
				marker, it.title, it.restart ? " (restart)" : "" );
			titleBuf[sizeof(titleBuf) - 1] = 0;
			m_pRowLabels[k]->setText( titleBuf );
		}

		for ( int o = 0; o < nopt; o++ )
		{
			ColorButton *btn = m_pRowButtons[k][o];
			Label       *tly = m_pRowVoteTallies[k][o];
			if ( !btn ) continue;

			char voteSz[12];
			_snprintf( voteSz, sizeof(voteSz), "%d", counts[o] );
			voteSz[sizeof(voteSz) - 1] = 0;
			if ( tly )
				tly->setText( voteSz );

			btn->setArmed( false );
			btn->setUnArmedColor( r, g, b, 0 );

			bool isWinner = ( hi == o && counts[o] > 0 );
			bool isMine   = ( myPick == o );

			if ( isMine )
				btn->setArmed( true );

			Color borderColor;
			if ( isMine )
			{
				borderColor = Color( 255, 255, 255, a );
				if ( tly )
					tly->setFgColor(255, 255, 255, 0);
			}
			else
			{
				if ( isWinner )
					borderColor = Color( 0, 255, 0, a );
				else
					borderColor = Color( r, g, b, a );
			}
			if ( m_pRowButtonBorders[k][o] )
			{
				int br, bg, bb, ba;
				borderColor.getColor( br, bg, bb, ba );
				m_pRowButtonBorders[k][o]->setLineColor( br, bg, bb, ba );
				btn->setBorder( m_pRowButtonBorders[k][o] );
			}

			if ( counts[o] > 0 )
			{
				btn->setArmed( true );
				btn->setBgColor(r, g, b, 255);
				if ( isMine )
					btn->setArmedColor(255, 255, 255, 0);
				else
					btn->setArmedColor(r, g, b, 0);
			}
			else
			{
				if ( tly ) tly->setFgColor( r, g, b, 0 );
			}
		}
	}

	// RTV grace: hide about 2.0s after the local player has voted every row.
	if ( allVoted )
	{
		if ( m_fAutoCloseTime <= 0 )
			m_fAutoCloseTime = gHUD.m_flTime + 2.0f;
		else if ( gHUD.m_flTime >= m_fAutoCloseTime )
		{
			gViewPort->HideVGUIMenu();
			return;
		}
	}
	else
	{
		m_fAutoCloseTime = 0;
	}

	char hdr[160];
	_snprintf( hdr, sizeof(hdr),
		"Bot Options for %s | Items: %d | Time Left: %.1f",
		g_szBotOptionsVoteModeClient, rowCount, timeLeft );
	hdr[sizeof(hdr) - 1] = 0;
	m_pTitleLabel->setText( hdr );
}

bool CVoteBotOptionsPanel::SlotInput( int iSlot )
{
	return false;
}

void CVoteBotOptionsPanel::Open( void )
{
	BuildRows();
	Update();
	CMenuPanel::Open();
}

void CVoteBotOptionsPanel::Initialize( void )
{
	setVisible( false );
	g_bBotOptionsReceived = false;
	g_iBotOptionsRevisionClient = -1;
	g_iBotOptionsVoteRevisionClient = -1;
	g_iBotOptionsClientCount = 0;
	g_iActiveBotOptionsClientCount = 0;
	g_bBotOptsResendRequested = false;
	g_bBotOptionsAutoCloseOnComplete = false;
	memset( g_PlayerBotOptVote, -1, sizeof(g_PlayerBotOptVote) );
}
