/*
==============
vgui_VoteGameOptionsWindow.cpp

Implements CVoteGameOptionsPanel -- the dynamic game-options vote panel.

Each row corresponds to one entry in g_iActiveGameOptionsClient[], which is
populated by MsgFunc_VoteOpts and indexes into the parsed g_GameOptionsClient[]
manifest (sent earlier via MsgFunc_GameOpts on connect or on revision change).

Row layout: [TitleLabel-west .... [opt1][opt2]...[optN]-east], one row per
active item. Clicking an option button issues "voteopt <item> <option>\n"
where both indices are 1-based (matches the server's ClientCommand handler).

The panel is rebuilt at Open() time. We do not keep static slots like the
mutator panel does because the row count and option counts are entirely
server-driven and can differ between games.
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

// Layout constants (mirrors VoteMutatorWindow magnitude).
#define GOMENU_TITLE_X				XRES(80)
#define GOMENU_TITLE_Y				YRES(32)
#define GOMENU_SCROLL_X				XRES(80)
#define GOMENU_SCROLL_Y				YRES(80)
#define GOMENU_SCROLL_WIDE			XRES(480)
#define GOMENU_SCROLL_TALL			YRES(340)
#define GOMENU_ROW_HEIGHT			YRES(36)
#define GOMENU_ROW_SPACER_Y			YRES(4)
#define GOMENU_LABEL_WIDE			XRES(110)
#define GOMENU_BUTTON_WIDE			XRES(80)
#define GOMENU_BUTTON_GAP			XRES(4)

// CMenuHandler_StringCommand fires a fixed string ("voteopt R O\n") at the engine.
// We allocate one per (row, option) cell and let VGUI own them.

CVoteGameOptionsPanel::CVoteGameOptionsPanel( int iTrans, int iRemoveMe, int x, int y, int wide, int tall )
	: CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );

	int r, g, b, a;

	m_pTitleLabel = new Label( "", GOMENU_TITLE_X, GOMENU_TITLE_Y );
	m_pTitleLabel->setParent( this );
	m_pTitleLabel->setFont( pSchemes->getFont( hTitleScheme ) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	m_pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	m_pTitleLabel->setBgColor( r, g, b, a );
	m_pTitleLabel->setContentAlignment( vgui::Label::a_west );
	m_pTitleLabel->setText( "Game Options Vote" );

	m_pScrollPanel = new CTFScrollPanel( GOMENU_SCROLL_X, GOMENU_SCROLL_Y, GOMENU_SCROLL_WIDE, GOMENU_SCROLL_TALL );
	m_pScrollPanel->setParent( this );
	m_pScrollPanel->setScrollBarAutoVisible( true, true );
	m_pScrollPanel->setScrollBarVisible( false, true );
	m_pScrollPanelBorder = new LineBorder( Color( r, g, b, 255 ) );
	m_pScrollPanel->setBorder( m_pScrollPanelBorder );
	m_pScrollPanel->validate();

	m_iRowCount = 0;
	m_fAutoCloseTime = 0;
	for ( int k = 0; k < MAX_CLIENT_GAME_OPTIONS; k++ )
	{
		m_pRowLabels[k] = NULL;
		m_iLocalPick[k] = -1;
		for ( int o = 0; o < MAX_CLIENT_GAME_OPTION_VALUES; o++ )
		{
			m_pRowButtons[k][o]    = NULL;
			m_pRowVoteTallies[k][o] = NULL;
		}
	}
}

// Rebuild the row widgets from g_iActiveGameOptionsClient[]. Called once
// per Open(). On stale-manifest (G4 path) we render a placeholder row so
// the timer still shows.
void CVoteGameOptionsPanel::BuildRows( void )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );

	int r, g, b, a;
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );

	// Tear down any prior widgets.
	for ( int k = 0; k < MAX_CLIENT_GAME_OPTIONS; k++ )
	{
		if ( m_pRowLabels[k] )
		{
			m_pRowLabels[k]->setVisible( false );
			m_pRowLabels[k] = NULL;
		}
		for ( int o = 0; o < MAX_CLIENT_GAME_OPTION_VALUES; o++ )
		{
			if ( m_pRowButtons[k][o] )
			{
				m_pRowButtons[k][o]->setVisible( false );
				m_pRowButtons[k][o] = NULL;
			}
			if ( m_pRowVoteTallies[k][o] )
			{
				m_pRowVoteTallies[k][o]->setVisible( false );
				m_pRowVoteTallies[k][o] = NULL;
			}
		}
	}

	m_iRowCount = g_iActiveGameOptionsClientCount;
	if ( m_iRowCount > MAX_CLIENT_GAME_OPTIONS )
		m_iRowCount = MAX_CLIENT_GAME_OPTIONS;

	// G4: if manifest hasn't arrived, render a wait-row.
	if ( !g_bGameOptionsReceived || m_iRowCount == 0 )
	{
		Label *pWait = new Label( "Waiting for game-options manifest...",
			GOMENU_ROW_SPACER_Y, GOMENU_ROW_SPACER_Y );
		pWait->setParent( m_pScrollPanel->getClient() );
		pWait->setFont( pSchemes->getFont( hClassWindowText ) );
		pWait->setSize( GOMENU_SCROLL_WIDE - XRES(20), GOMENU_ROW_HEIGHT );
		pWait->setContentAlignment( vgui::Label::a_west );
		pWait->setFgColor( r, g, b, 0 );
		pWait->setBgColor( 0, 0, 0, 255 );
		m_pRowLabels[0] = pWait;
		m_pScrollPanel->validate();
		return;
	}

	for ( int k = 0; k < m_iRowCount; k++ )
	{
		int realIdx = g_iActiveGameOptionsClient[k];
		if ( realIdx < 0 || realIdx >= g_iGameOptionsClientCount )
			continue;
		const client_game_option_t &it = g_GameOptionsClient[realIdx];

		int yPos = GOMENU_ROW_SPACER_Y + k * ( GOMENU_ROW_HEIGHT + GOMENU_ROW_SPACER_Y );

		// Title label on the left.
		m_pRowLabels[k] = new Label( "", GOMENU_ROW_SPACER_Y, yPos );
		m_pRowLabels[k]->setParent( m_pScrollPanel->getClient() );
		m_pRowLabels[k]->setFont( pSchemes->getFont( hTitleScheme ) );
		m_pRowLabels[k]->setSize( GOMENU_LABEL_WIDE, GOMENU_ROW_HEIGHT );
		m_pRowLabels[k]->setContentAlignment( vgui::Label::a_west );
		m_pRowLabels[k]->setFgColor( r, g, b, 0 );
		m_pRowLabels[k]->setBgColor( 0, 0, 0, 255 );

		char titleBuf[80];
		_snprintf( titleBuf, sizeof(titleBuf), " %s%s",
			it.title, it.restart ? " (restart)" : "" );
		titleBuf[sizeof(titleBuf) - 1] = 0;
		m_pRowLabels[k]->setText( titleBuf );

		// Option buttons to the right of the label.
		int nopt = it.numOptions;
		if ( nopt > MAX_CLIENT_GAME_OPTION_VALUES )
			nopt = MAX_CLIENT_GAME_OPTION_VALUES;

		for ( int o = 0; o < nopt; o++ )
		{
			int xPos = GOMENU_ROW_SPACER_Y + GOMENU_LABEL_WIDE
			         + o * ( GOMENU_BUTTON_WIDE + GOMENU_BUTTON_GAP );

			char cmdBuf[24];
			_snprintf( cmdBuf, sizeof(cmdBuf), "voteopt %d %d\n", k + 1, o + 1 );
			cmdBuf[sizeof(cmdBuf) - 1] = 0;
			ActionSignal *pASig = new CMenuHandler_StringCommand( cmdBuf, false );

			m_pRowButtons[k][o] = new ColorButton( it.labels[o],
				xPos, yPos, GOMENU_BUTTON_WIDE, GOMENU_ROW_HEIGHT, false, true );
			m_pRowButtons[k][o]->setBoundKey( (char)255 );
			m_pRowButtons[k][o]->setContentAlignment( vgui::Label::a_northwest );
			m_pRowButtons[k][o]->addActionSignal( pASig );
			m_pRowButtons[k][o]->setParent( m_pScrollPanel->getClient() );
			m_pRowButtons[k][o]->setFont( pSchemes->getFont( hTitleScheme ) );

			m_pRowVoteTallies[k][o] = new Label( "0",
				GOMENU_BUTTON_WIDE - XRES(18), YRES(2) );
			m_pRowVoteTallies[k][o]->setParent( m_pRowButtons[k][o] );
			m_pRowVoteTallies[k][o]->setFont( pSchemes->getFont( hTitleScheme ) );
			m_pRowVoteTallies[k][o]->setContentAlignment( vgui::Label::a_east );
			m_pRowVoteTallies[k][o]->setSize( XRES(14), YRES(14) );
			m_pRowVoteTallies[k][o]->setFgColor( r, g, b, 0 );
			m_pRowVoteTallies[k][o]->setBgColor( 0, 0, 0, 255 );
		}
	}

	m_pScrollPanel->validate();
}

void CVoteGameOptionsPanel::Update()
{
	int seconds = (int)fmax( 0, ( m_iTime + m_fStartTime - gHUD.m_flTime ) );

	int r, g, b, a = 0;
	UnpackRGB( r, g, b, HudColor() );

	if ( m_pScrollPanelBorder )
	{
		m_pScrollPanelBorder->setColor( Color( r, g, b, 255 ) );
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

	// Track whether the local player has cast a vote on every active row;
	// if so, auto-close the panel after a short grace period so they can see
	// the final tally state (mirrors the mutator-RTV menu's dismiss flow).
	bool allVoted = ( m_iRowCount > 0 );

	// Per-row tally update.
	for ( int k = 0; k < m_iRowCount; k++ )
	{
		int realIdx = g_iActiveGameOptionsClient[k];
		if ( realIdx < 0 || realIdx >= g_iGameOptionsClientCount )
			continue;
		const client_game_option_t &it = g_GameOptionsClient[realIdx];
		int nopt = it.numOptions;
		if ( nopt > MAX_CLIENT_GAME_OPTION_VALUES )
			nopt = MAX_CLIENT_GAME_OPTION_VALUES;

		// Find leader for this row.
		int counts[MAX_CLIENT_GAME_OPTION_VALUES] = { 0 };
		for ( int p = 1; p <= MAX_PLAYERS; p++ )
		{
			int v = g_PlayerOptVote[p][k];
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
				myPick = g_PlayerOptVote[p][k];
				break;
			}
		}
		if ( myPick < 0 )
			allVoted = false;

		// Two independent visual cues:
		//  1. Row title prepends [X]/[ ] so the local player can see at a
		//     glance which rows still need a vote.
		//  2. Per-button highlight uses border color for the local pick
		//     (yellow) so it stays visible whether or not it is also the
		//     leading option, which keeps the existing fill semantics
		//     (solid blue = winner) intact.
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

			// Border channel: mine wins over winner so the pick is always
			// visible. Yellow contrasts against both the blue fill and the
			// white winner border.
			Color borderColor;
			if ( isMine )
				borderColor = Color( 255, 255, 0, a );
			else if ( isWinner )
				borderColor = Color( 255, 255, 255, a );
			else
				borderColor = Color( r, g, b, a );
			btn->setBorder( new LineBorder( borderColor ) );

			if ( counts[o] > 0 )
			{
				btn->setArmed( true );
				if ( isWinner )
				{
					btn->setBgColor( r, g, b, 0 );
					btn->setArmedColor( 255, 255, 255, 0 );
					if ( tly ) tly->setFgColor( 255, 255, 255, 0 );
				}
				else
				{
					btn->setBgColor( r, g, b, 255 );
					btn->setArmedColor( r, g, b, 0 );
					if ( tly ) tly->setFgColor( r, g, b, 0 );
				}
			}
			else
			{
				if ( tly ) tly->setFgColor( r, g, b, 0 );
			}
		}
	}

	// Auto-close once the local player has voted on every active row.
	// 1.5s grace gives the user time to see the final tally state before
	// the panel disappears. The server-side vote timer keeps running and
	// the tally still applies as usual when it expires.
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
		// Player revised a vote back to nothing somehow; cancel the timer.
		m_fAutoCloseTime = 0;
	}

	char hdr[160];
	_snprintf( hdr, sizeof(hdr),
		"Game Options Vote | Items: %d | Time Left: %d",
		m_iRowCount, seconds );
	hdr[sizeof(hdr) - 1] = 0;
	m_pTitleLabel->setText( hdr );
}

bool CVoteGameOptionsPanel::SlotInput( int iSlot )
{
	return false;  // numeric slots don't map cleanly to 2-D voteopt; no-op.
}

void CVoteGameOptionsPanel::Open( void )
{
	BuildRows();
	Update();
	CMenuPanel::Open();
}

void CVoteGameOptionsPanel::Initialize( void )
{
	setVisible( false );
}

void CVoteGameOptionsPanel::OnRowButton( int row, int option )
{
	// Reserved for future direct-callback wiring; ColorButton uses
	// CMenuHandler_StringCommand pre-built commands instead.
	m_iLocalPick[row] = option;
}
