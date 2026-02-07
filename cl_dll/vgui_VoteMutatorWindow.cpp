
#include "VGUI_Font.h"
#include <VGUI_TextImage.h>

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

// Menu Dimensions
#define MUTATORMENU_TITLE_X				XRES(40)
#define MUTATORMENU_TITLE_Y				YRES(32)
#define MUTATORMENU_SCROLL_X			XRES(40)
#define MUTATORMENU_SCROLL_Y			YRES(80)
#define MUTATORMENU_SCROLL_WIDE			XRES(560)
#define MUTATORMENU_SCROLL_TALL			YRES(340)
#define MUTATORMENU_NUM_COLUMNS			2
#define MUTATORMENU_BUTTON_SIZE_X		XRES(268)
#define MUTATORMENU_BUTTON_SIZE_Y		YRES(36)
#define MUTATORMENU_BUTTON_SPACER_Y		YRES(4)
#define MUTATORMENU_BUTTON_SPACER_X		XRES(8)

// Creation
CVoteMutatorPanel::CVoteMutatorPanel(int iTrans, int iRemoveMe, int x,int y,int wide,int tall) : CMenuPanel(iTrans, iRemoveMe, x,y,wide,tall)
{
	// Get the scheme used for the Titles
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	// schemes
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );

	// color schemes
	int r, g, b, a;

	// Create the title
	pTitleLabel = new Label( "", MUTATORMENU_TITLE_X, MUTATORMENU_TITLE_Y );
	pTitleLabel->setParent( this );
	pTitleLabel->setFont( pSchemes->getFont(hTitleScheme) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setBgColor( r, g, b, a );
	pTitleLabel->setContentAlignment( vgui::Label::a_west );
	pTitleLabel->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMutator"));

	// Create the scroll panel
	m_pScrollPanel = new CTFScrollPanel( MUTATORMENU_SCROLL_X, MUTATORMENU_SCROLL_Y, MUTATORMENU_SCROLL_WIDE, MUTATORMENU_SCROLL_TALL );
	m_pScrollPanel->setParent(this);
	m_pScrollPanel->setScrollBarAutoVisible(true, true);
	m_pScrollPanel->setScrollBarVisible(false, true);
	m_pScrollPanel->setBorder( new LineBorder( Color(r, g, b, 0)) );
	m_pScrollPanel->validate();

	// Create the buttons inside scroll panel
	int positionCount = 0;
	for (int i = 0; i < MAX_MUTATORS; i++)
	{
		m_pButtons[i] = NULL;
		if (strstr(sMutators[i].name, "slowmo") ||
			strstr(sMutators[i].name, "speedup") ||
			strstr(sMutators[i].name, "topsyturvy") ||
			strstr(sMutators[i].name, "itemsexplode") ||
			strstr(sMutators[i].name, "explosiveai"))
		{
			continue;
		}

		// Calculate position in grid layout
		int column, row;
		
		// Special handling for "Random" button (last mutator) - always first position
		if (i == MAX_MUTATORS - 1)
		{
			row = 0;
			column = 0;
		}
		else
		{
			// Regular buttons start at position 1 to leave space for Random at position 0
			column = (positionCount + 1) % 2;
			row = (positionCount + 1) / 2;
			positionCount++;
		}
		
		int iXPos = MUTATORMENU_BUTTON_SPACER_X + (column * (MUTATORMENU_BUTTON_SIZE_X + MUTATORMENU_BUTTON_SPACER_X));
		int iYPos = MUTATORMENU_BUTTON_SPACER_Y + (row * (MUTATORMENU_BUTTON_SIZE_Y + MUTATORMENU_BUTTON_SPACER_Y));
		
		char voteCommand[16];
		sprintf(voteCommand, "vote %d", i+1);
		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect(voteCommand, false );

		// mutator buttons
		char sz[256];
		sprintf(sz, " %s", sMutators[i].name);
		m_pButtons[i] = new ColorButton( sz, iXPos, iYPos, MUTATORMENU_BUTTON_SIZE_X, MUTATORMENU_BUTTON_SIZE_Y, false, true);
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setParent( m_pScrollPanel->getClient() );
		
		// Add subtitle label as a child of the button
		Label *pSubtitle = new Label( "", XRES(10), MUTATORMENU_BUTTON_SIZE_Y - YRES(14) );
		pSubtitle->setParent( m_pButtons[i] );
		pSubtitle->setFont( Scheme::sf_primary3 );
		pSubtitle->setContentAlignment( vgui::Label::a_west );
		
		// Use scheme color for subtitle (dimmed for visual hierarchy)
		int sr = 255, sg = 255, sb = 255, sa = 125;
		//pSchemes->getFgColor( hClassWindowText, sr, sg, sb, sa );
		pSubtitle->setFgColor( sr, sg, sb, sa );
		pSubtitle->setBgColor( 0, 0, 0, 255 );
		pSubtitle->setText( sMutators[i].description );
	}
	
	// Validate scroll panel after adding all buttons
	m_pScrollPanel->validate();

	m_iCurrentInfo = 0;
}

void CVoteMutatorPanel::Update()
{
	// Time
	float minutes = fmax( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) / 60);
	float seconds = fmax( 0, ( m_iTime + m_fStartTime - gHUD.m_flTime ) - (minutes * 60));

	int votes[MAX_MUTATORS];
	int myVote = -1;

	// Count votes
	for (int j = 0; j < MAX_MUTATORS; j++)
	{
		if (m_pButtons[j])
			m_pButtons[j]->setArmed(false);
	}

	// Count votes
	int highest = -1, hi = -1;
	for ( int i = 0; i < MAX_MUTATORS; i++ )
	{
		votes[i] = 0;
		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			if ( g_PlayerInfoList[j].thisplayer )
				myVote = g_PlayerExtraInfo[j].vote;

			if (g_PlayerExtraInfo[j].vote == (i + 1))
				votes[i] += 1;
		}

		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			if (highest < votes[i])
			{
				highest = votes[i];
				hi = i;
			}
		}
	}

	int second = -1, s = -1;

	for ( int i = 0; i < MAX_MUTATORS; i++ )
	{
		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			if (second < votes[i] && second <= highest && i != hi)
			{
				second = votes[i];
				s = i;
			}
		}
	}

	int third = -1, t = -1;
	for ( int i = 0; i < MAX_MUTATORS; i++ )
	{
		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			if (third < votes[i] && third <= second && i != hi && i != s)
			{
				third = votes[i];
				t = i;
			}
		}
	}

	for ( int i = 0; i < MAX_MUTATORS; i++ )
	{
		if (m_pButtons[i])
		{
			char sz[64];
			sprintf(sz, " %-2d %s", votes[i], sMutators[i].name);
			m_pButtons[i]->setText(sz);

			if ((myVote - 1) == i)
			{
				m_pButtons[i]->setArmed(true);
			}

			int r, g, b, a = 0;
			UnpackRGB(r, g, b, HudColor());
			m_pButtons[i]->setUnArmedColor(r, g, b, 0);
			pTitleLabel->setFgColor( r, g, b, 0 );
			if (votes[i] > 0)
			{
				m_pButtons[i]->setArmed(true);
				if (i == hi || i == s || i == t)
				{
					m_pButtons[i]->setBorder(new LineBorder(Color(255, 255, 255, a)));
					m_pButtons[i]->setBgColor(r, g, b, 0);
					m_pButtons[i]->setArmedColor(255, 255, 255, 0);
				}
				else
				{
					m_pButtons[i]->setBorder(new LineBorder(Color(r, g, b, a)));
					m_pButtons[i]->setBgColor(r, g, b, 255);
					m_pButtons[i]->setArmedColor(r, g, b, 0);
				}
			}
			else
			{
				m_pButtons[i]->setBorder(new LineBorder( Color(r, g, b, a)));
			}
		}
	}

	pTitleLabel->setText("%s | Your Vote: %s | Time Left: %.1f\n",
		gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMutator"),
		myVote > 0 ? sMutators[myVote-1].name : "None", seconds);
}

//======================================
// Key inputs for the Class Menu
bool CVoteMutatorPanel::SlotInput( int iSlot )
{
	if ( (iSlot < 0) || (iSlot > 9) )
		return false;

	if ( !m_pButtons[ iSlot ] )
		return false;

	//if ( !(m_pButtons[ iSlot ]->IsNotValid()) )
	{
		for (int i = 0; i < MAX_MUTATORS; i++)
		{
			if (m_pButtons[i])
				m_pButtons[i]->setArmed( false );
		}

		m_pButtons[ iSlot ]->setArmed( true );
		m_iCurrentInfo = iSlot;
		m_pButtons[ iSlot ]->fireActionSignal();
		return true;
	}

	return false;
}

//======================================
// Update the Class menu before opening it
void CVoteMutatorPanel::Open( void )
{
	SetActiveInfo(0);
	Update();
	CMenuPanel::Open();
}

//-----------------------------------------------------------------------------
// Purpose: Called each time a new level is started.
//-----------------------------------------------------------------------------
void CVoteMutatorPanel::Initialize( void )
{
	setVisible( false );
}

//======================================
// Mouse is over a class button, bring up the class info
void CVoteMutatorPanel::SetActiveInfo( int iInput )
{
	if ( iInput > (MAX_MUTATORS - 1) || iInput < 0 )
		iInput = 0;

	m_iCurrentInfo = iInput;
}
