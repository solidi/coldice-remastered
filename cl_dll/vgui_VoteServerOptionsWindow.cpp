/*
==============
vgui_VoteServerOptionsWindow.cpp

Implements CVoteServerOptionsPanel -- the dynamic server-options vote panel.
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

#define SOMENU_TITLE_X              XRES(80)
#define SOMENU_TITLE_Y              YRES(32)
#define SOMENU_SCROLL_X             XRES(80)
#define SOMENU_SCROLL_Y             YRES(80)
#define SOMENU_SCROLL_WIDE          XRES(480)
#define SOMENU_SCROLL_TALL          YRES(340)
#define SOMENU_ROW_HEIGHT           YRES(36)
#define SOMENU_ROW_SPACER_Y         YRES(4)
#define SOMENU_LABEL_WIDE           XRES(140)
#define SOMENU_BUTTON_WIDE          XRES(80)
#define SOMENU_BUTTON_GAP           XRES(4)

CVoteServerOptionsPanel::CVoteServerOptionsPanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall )
	: CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );

	int r, g, b, a;

	m_pTitleLabel = new Label( "", SOMENU_TITLE_X, SOMENU_TITLE_Y );
	m_pTitleLabel->setParent( this );
	m_pTitleLabel->setFont( pSchemes->getFont( hTitleScheme ) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	m_pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	m_pTitleLabel->setBgColor( r, g, b, a );
	m_pTitleLabel->setContentAlignment( vgui::Label::a_west );
	m_pTitleLabel->setText( "Server Options for ?" );

	m_pScrollPanel = new CTFScrollPanel( SOMENU_SCROLL_X, SOMENU_SCROLL_Y, SOMENU_SCROLL_WIDE, SOMENU_SCROLL_TALL );
	m_pScrollPanel->setParent( this );
	m_pScrollPanel->setScrollBarAutoVisible( true, true );
	m_pScrollPanel->setScrollBarVisible( false, true );
	m_pScrollPanelBorder = new LineBorder( Color( r, g, b, 255 ) );
	m_pScrollPanel->setBorder( m_pScrollPanelBorder );
	m_pScrollPanel->validate();

	m_iRowCount = 0;
	m_fAutoCloseTime = 0;
	for ( int k = 0; k < MAX_CLIENT_SERVER_OPTIONS; k++ )
	{
		m_pRowLabels[k] = NULL;
		m_iLocalPick[k] = -1;
		for ( int o = 0; o < MAX_CLIENT_SERVER_OPTION_VALUES; o++ )
		{
			m_pRowButtons[k][o] = NULL;
			m_pRowButtonBorders[k][o] = NULL;
			m_pRowVoteTallies[k][o] = NULL;
		}
	}
}

void CVoteServerOptionsPanel::BuildRows( void )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );
	SchemeHandle_t hPrimaryButtonText = pSchemes->getSchemeHandle( "Primary Button Text" );

	int r, g, b, a;
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );

	for ( int k = 0; k < MAX_CLIENT_SERVER_OPTIONS; k++ )
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
			if ( m_pRowButtonBorders[k][o] )
			{
				m_pRowButtonBorders[k][o] = NULL;
			}
			if ( m_pRowVoteTallies[k][o] )
			{
				m_pRowVoteTallies[k][o]->setVisible( false );
				m_pRowVoteTallies[k][o] = NULL;
			}
		}
	}

	m_iRowCount = g_iActiveServerOptionsClientCount;
	if ( m_iRowCount > MAX_CLIENT_SERVER_OPTIONS )
		m_iRowCount = MAX_CLIENT_SERVER_OPTIONS;

	if ( !g_bServerOptionsReceived || m_iRowCount == 0 )
	{
		Label *pWait = new Label( "Waiting for server-options manifest...",
			SOMENU_ROW_SPACER_Y, SOMENU_ROW_SPACER_Y );
		pWait->setParent( m_pScrollPanel->getClient() );
		pWait->setFont( pSchemes->getFont( hClassWindowText ) );
		pWait->setSize( SOMENU_SCROLL_WIDE - XRES(20), SOMENU_ROW_HEIGHT );
		pWait->setContentAlignment( vgui::Label::a_west );
		pWait->setFgColor( r, g, b, 0 );
		pWait->setBgColor( 0, 0, 0, 255 );
		m_pRowLabels[0] = pWait;
		m_pScrollPanel->validate();
		return;
	}

	for ( int k = 0; k < m_iRowCount; k++ )
	{
		int realIdx = g_iActiveServerOptionsClient[k];
		if ( realIdx < 0 || realIdx >= g_iServerOptionsClientCount )
			continue;
		const client_server_option_t &it = g_ServerOptionsClient[realIdx];

		int yPos = SOMENU_ROW_SPACER_Y + k * ( SOMENU_ROW_HEIGHT + SOMENU_ROW_SPACER_Y );

		m_pRowLabels[k] = new Label( "", SOMENU_ROW_SPACER_Y, yPos );
		m_pRowLabels[k]->setParent( m_pScrollPanel->getClient() );
		m_pRowLabels[k]->setFont( pSchemes->getFont( ScreenWidth >= 1024 ? hTitleScheme : hPrimaryButtonText ) );
		m_pRowLabels[k]->setSize( SOMENU_LABEL_WIDE, SOMENU_ROW_HEIGHT );
		m_pRowLabels[k]->setContentAlignment( vgui::Label::a_west );
		m_pRowLabels[k]->setFgColor( r, g, b, 0 );
		m_pRowLabels[k]->setBgColor( 0, 0, 0, 255 );

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
			int xPos = SOMENU_ROW_SPACER_Y + SOMENU_LABEL_WIDE
			         + o * ( SOMENU_BUTTON_WIDE + SOMENU_BUTTON_GAP );

			char cmdBuf[28];
			_snprintf( cmdBuf, sizeof(cmdBuf), "votesrvopt %d %d\n", k + 1, o + 1 );
			cmdBuf[sizeof(cmdBuf) - 1] = 0;
			ActionSignal *pASig = new CMenuHandler_StringCommand( cmdBuf, false );

			vgui::Font *font = pSchemes->getFont(hPrimaryButtonText);
			if (ScreenWidth <= 1024)
				font = pSchemes->getFont(hClassWindowText);

			char sz[64];
			sprintf(sz, " %s", it.labels[o]);
			m_pRowButtons[k][o] = new ColorButton( sz,
				xPos, yPos, SOMENU_BUTTON_WIDE, SOMENU_ROW_HEIGHT, false, true );
			m_pRowButtonBorders[k][o] = new LineBorder( Color(r, g, b, a) );
			m_pRowButtons[k][o]->setBorder( m_pRowButtonBorders[k][o] );
			m_pRowButtons[k][o]->setBoundKey( (char)255 );
			m_pRowButtons[k][o]->setContentAlignment( vgui::Label::a_west );
			m_pRowButtons[k][o]->addActionSignal( pASig );
			m_pRowButtons[k][o]->setParent( m_pScrollPanel->getClient() );
			m_pRowButtons[k][o]->setFont( font );

			m_pRowVoteTallies[k][o] = new Label( "0",
				SOMENU_BUTTON_WIDE - XRES(22), SOMENU_ROW_HEIGHT / 2.5f );
			m_pRowVoteTallies[k][o]->setParent( m_pRowButtons[k][o] );
			m_pRowVoteTallies[k][o]->setFont( font );
			m_pRowVoteTallies[k][o]->setContentAlignment( vgui::Label::a_northeast );
			m_pRowVoteTallies[k][o]->setFgColor( r, g, b, 0 );
			m_pRowVoteTallies[k][o]->setBgColor( 0, 0, 0, 255 );
		}
	}

	m_pScrollPanel->validate();
}

void CVoteServerOptionsPanel::Update()
{
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

	bool allVoted = ( g_bServerOptionsAutoCloseOnComplete && g_bServerOptionsReceived && ( m_iRowCount > 0 ) );
	for ( int k = 0; k < m_iRowCount; k++ )
	{
		int realIdx = g_iActiveServerOptionsClient[k];
		if ( realIdx < 0 || realIdx >= g_iServerOptionsClientCount )
			continue;
		const client_server_option_t &it = g_ServerOptionsClient[realIdx];
		int nopt = it.numOptions;
		if ( nopt > MAX_CLIENT_SERVER_OPTION_VALUES )
			nopt = MAX_CLIENT_SERVER_OPTION_VALUES;

		int counts[MAX_CLIENT_SERVER_OPTION_VALUES] = { 0 };
		for ( int p = 1; p <= MAX_PLAYERS; p++ )
		{
			int v = g_PlayerSrvOptVote[p][k];
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
				myPick = g_PlayerSrvOptVote[p][k];
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

	if ( allVoted )
	{
		if ( m_fAutoCloseTime <= 0 )
			m_fAutoCloseTime = gHUD.m_flTime + 1.5f;
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
		"Server Options for %s | Items: %d | Time Left: %.1f",
		g_szServerOptionsVoteModeClient, m_iRowCount, timeLeft );
	hdr[sizeof(hdr) - 1] = 0;
	m_pTitleLabel->setText( hdr );
}

bool CVoteServerOptionsPanel::SlotInput( int iSlot )
{
	return false;
}

void CVoteServerOptionsPanel::Open( void )
{
	BuildRows();
	Update();
	CMenuPanel::Open();
}

void CVoteServerOptionsPanel::Initialize( void )
{
	setVisible( false );
}

void CVoteServerOptionsPanel::OnRowButton( int row, int option )
{
	m_iLocalPick[row] = option;
}
