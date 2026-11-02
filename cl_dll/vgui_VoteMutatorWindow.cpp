
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

// Menu Dimensions
#define MUTATORMENU_TITLE_X				XRES(80)
#define MUTATORMENU_TITLE_Y				YRES(32)
#define MUTATORMENU_SCROLL_X			XRES(80)
#define MUTATORMENU_SCROLL_Y			YRES(80)
#define MUTATORMENU_SCROLL_WIDE			XRES(480)
#define MUTATORMENU_SCROLL_TALL			YRES(340)
#define MUTATORMENU_NUM_COLUMNS			2
#define MUTATORMENU_BUTTON_SIZE_X		XRES(222)
#define MUTATORMENU_BUTTON_SIZE_Y		YRES(36)
#define MUTATORMENU_BUTTON_SPACER_Y		YRES(4)
#define MUTATORMENU_BUTTON_SPACER_X		XRES(8)

// Vote slots (0-based = vote ID - 1): real mutators + RANDOM, then the
// synthetic INSTANT MUTATORS and CLEAR slots (vote IDs in const.h).
#define MUTATOR_MENU_TOTAL_SLOTS        MUTATOR_VOTE_SLOTS
#define MUTATOR_MENU_CLEAR_SLOT         (MUTATOR_VOTE_CLEAR - 1)

// Synthetic buttons, created in this order right after CHAOS: INSTANT takes
// row 1 left, CLEAR row 1 right (under CHAOS); every mutator follows them.
static const struct
{
	int voteId;
	const char *title;
	const char *subtitle;
} s_MutatorSpecialSlots[MUTATOR_MENU_SPECIAL_SLOTS] =
{
	{ MUTATOR_VOTE_INSTANT, "INSTANT", "swapped stats, tripmines, etc." },
	{ MUTATOR_VOTE_CLEAR,   "CLEAR",   "No mutators, please." },
};

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
	pTitleLabel->setText("Mutator Vote for ?");

	// Create the scroll panel
	m_pScrollPanel = new CTFScrollPanel( MUTATORMENU_SCROLL_X, MUTATORMENU_SCROLL_Y, MUTATORMENU_SCROLL_WIDE, MUTATORMENU_SCROLL_TALL );
	m_pScrollPanel->setParent(this);
	m_pScrollPanel->setScrollBarAutoVisible(true, true);
	m_pScrollPanel->setScrollBarVisible(false, true);
	m_pScrollPanelBorder = new LineBorder( Color(r, g, b, 255) );
	m_pScrollPanel->setBorder( m_pScrollPanelBorder );
	m_pScrollPanel->validate();
	m_pScrollPanel->addInputSignal( new CHandler_MenuWheelForward(this) );
	m_pScrollPanel->getClient()->addInputSignal( new CHandler_MenuWheelForward(this) );

	// Create the buttons inside scroll panel
	int positionCount = 0;
	for (int k = 0; k < MUTATOR_MENU_SPECIAL_SLOTS; k++)
	{
		m_pSpecialButtons[k] = NULL;
		m_pSpecialButtonBorders[k] = NULL;
		m_pSpecialVoteTallyLabels[k] = NULL;
	}
	for (int i = 0; i < MAX_MUTATORS_CL; i++)
	{
		m_pButtons[i] = NULL;
		m_pButtonBorders[i] = NULL;
		m_pVoteTallyLabels[i] = NULL;
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
		if (i == MAX_MUTATORS_CL - 1)
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
		m_pButtonBorders[i] = new LineBorder( Color(r, g, b, a) );
		m_pButtons[i]->setBorder( m_pButtonBorders[i] );
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setParent( m_pScrollPanel->getClient() );
		m_pButtons[i]->setFont( pSchemes->getFont(hTitleScheme) );
		
		// Add vote tally label (right-aligned, vertically centered)
		m_pVoteTallyLabels[i] = new Label( "0", MUTATORMENU_BUTTON_SIZE_X - XRES(25), YRES(10) );
		m_pVoteTallyLabels[i]->setParent( m_pButtons[i] );
		m_pVoteTallyLabels[i]->setFont( pSchemes->getFont(hTitleScheme) );
		m_pVoteTallyLabels[i]->setContentAlignment( vgui::Label::a_east );
		m_pVoteTallyLabels[i]->setSize( XRES(10), YRES(18) );
		pSchemes->getFgColor( hTitleScheme, r, g, b, a );
		m_pVoteTallyLabels[i]->setFgColor( r, g, b, a );
		m_pVoteTallyLabels[i]->setBgColor( 0, 0, 0, 255 );
		m_pVoteTallyLabels[i]->addInputSignal( new CHandler_MenuWheelForward(this) );
		
		// Add subtitle label as a child of the button
		Label *pSubtitle = new Label( "", XRES(5), MUTATORMENU_BUTTON_SIZE_Y - YRES(16) );
		pSubtitle->setParent( m_pButtons[i] );
		pSubtitle->setFont( pSchemes->getFont(hClassWindowText) );
		pSubtitle->setContentAlignment( vgui::Label::a_west );
		
		// Use scheme color for subtitle (dimmed for visual hierarchy)
		int sr = 255, sg = 255, sb = 255, sa = 75;
		//pSchemes->getFgColor( hClassWindowText, sr, sg, sb, sa );
		pSubtitle->setFgColor( sr, sg, sb, sa );
		pSubtitle->setBgColor( 0, 0, 0, 255 );
		pSubtitle->setText( sMutators[i].description );
		pSubtitle->addInputSignal( new CHandler_MenuWheelForward(this) );

		// Synthetic slots injected right after CHAOS.
		if ( i == 0 )
		{
			for ( int k = 0; k < MUTATOR_MENU_SPECIAL_SLOTS; k++ )
			{
				int specialColumn = (positionCount + 1) % 2;
				int specialRow = (positionCount + 1) / 2;
				positionCount++;

				int iXPosSpecial = MUTATORMENU_BUTTON_SPACER_X + (specialColumn * (MUTATORMENU_BUTTON_SIZE_X + MUTATORMENU_BUTTON_SPACER_X));
				int iYPosSpecial = MUTATORMENU_BUTTON_SPACER_Y + (specialRow * (MUTATORMENU_BUTTON_SIZE_Y + MUTATORMENU_BUTTON_SPACER_Y));

				char voteCommandSpecial[16];
				sprintf(voteCommandSpecial, "vote %d", s_MutatorSpecialSlots[k].voteId);
				ActionSignal *pASignalSpecial = new CMenuHandler_StringCommandClassSelect(voteCommandSpecial, false );

				m_pSpecialButtons[k] = new ColorButton( (char *)s_MutatorSpecialSlots[k].title, iXPosSpecial, iYPosSpecial,
					MUTATORMENU_BUTTON_SIZE_X, MUTATORMENU_BUTTON_SIZE_Y, false, true );
				m_pSpecialButtonBorders[k] = new LineBorder( Color(r, g, b, a) );
				m_pSpecialButtons[k]->setBorder( m_pSpecialButtonBorders[k] );
				m_pSpecialButtons[k]->setBoundKey( (char)255 );
				m_pSpecialButtons[k]->setContentAlignment( vgui::Label::a_northwest );
				m_pSpecialButtons[k]->addActionSignal( pASignalSpecial );
				m_pSpecialButtons[k]->addInputSignal( new CHandler_MenuButtonOver(this, s_MutatorSpecialSlots[k].voteId - 1) );
				m_pSpecialButtons[k]->setParent( m_pScrollPanel->getClient() );
				m_pSpecialButtons[k]->setFont( pSchemes->getFont(hTitleScheme) );

				m_pSpecialVoteTallyLabels[k] = new Label( "0", MUTATORMENU_BUTTON_SIZE_X - XRES(25), YRES(10) );
				m_pSpecialVoteTallyLabels[k]->setParent( m_pSpecialButtons[k] );
				m_pSpecialVoteTallyLabels[k]->setFont( pSchemes->getFont(hTitleScheme) );
				m_pSpecialVoteTallyLabels[k]->setContentAlignment( vgui::Label::a_east );
				m_pSpecialVoteTallyLabels[k]->setSize( XRES(10), YRES(18) );
				m_pSpecialVoteTallyLabels[k]->setFgColor( r, g, b, a );
				m_pSpecialVoteTallyLabels[k]->setBgColor( 0, 0, 0, 255 );
				m_pSpecialVoteTallyLabels[k]->addInputSignal( new CHandler_MenuWheelForward(this) );

				Label *pSpecialSubtitle = new Label( "", XRES(5), MUTATORMENU_BUTTON_SIZE_Y - YRES(16) );
				pSpecialSubtitle->setParent( m_pSpecialButtons[k] );
				pSpecialSubtitle->setFont( pSchemes->getFont(hClassWindowText) );
				pSpecialSubtitle->setContentAlignment( vgui::Label::a_west );
				pSpecialSubtitle->setFgColor( sr, sg, sb, sa );
				pSpecialSubtitle->setBgColor( 0, 0, 0, 255 );
				pSpecialSubtitle->setText( s_MutatorSpecialSlots[k].subtitle );
				pSpecialSubtitle->addInputSignal( new CHandler_MenuWheelForward(this) );
			}
		}
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

	int votes[MUTATOR_MENU_TOTAL_SLOTS];
	int myVote = -1;

	for (int j = 0; j < MAX_MUTATORS_CL; j++)
	{
		if (m_pButtons[j])
			m_pButtons[j]->setArmed(false);
	}
	for (int k = 0; k < MUTATOR_MENU_SPECIAL_SLOTS; k++)
	{
		if (m_pSpecialButtons[k])
			m_pSpecialButtons[k]->setArmed(false);
	}

	// Count votes (slot = vote ID - 1)
	memset(votes, 0, sizeof(votes));
	for ( int j = 1; j <= MAX_PLAYERS; j++ )
	{
		if ( g_PlayerInfoList[j].thisplayer )
			myVote = g_PlayerExtraInfo[j].vote;

		int slot = g_PlayerExtraInfo[j].vote - 1;
		if ( slot >= 0 && slot < MUTATOR_MENU_TOTAL_SLOTS )
			votes[slot] += 1;
	}

	// Top three outlined in green, as the server ranks them. CLEAR is not
	// ranked: when it ties or beats the leader it alone is outlined.
	int highest = -1, hi = -1;
	for ( int i = 0; i < MUTATOR_MENU_TOTAL_SLOTS; i++ )
	{
		if (i != MUTATOR_MENU_CLEAR_SLOT && highest < votes[i])
		{
			highest = votes[i];
			hi = i;
		}
	}

	int second = -1, s = -1;

	for ( int i = 0; i < MUTATOR_MENU_TOTAL_SLOTS; i++ )
	{
		if (i != MUTATOR_MENU_CLEAR_SLOT && second < votes[i] && second <= highest && i != hi)
		{
			second = votes[i];
			s = i;
		}
	}

	int third = -1, t = -1;
	for ( int i = 0; i < MUTATOR_MENU_TOTAL_SLOTS; i++ )
	{
		if (i != MUTATOR_MENU_CLEAR_SLOT && third < votes[i] && third <= second && i != hi && i != s)
		{
			third = votes[i];
			t = i;
		}
	}

	if ( votes[MUTATOR_MENU_CLEAR_SLOT] > 0 && votes[MUTATOR_MENU_CLEAR_SLOT] >= highest )
	{
		hi = MUTATOR_MENU_CLEAR_SLOT;
		s = t = -1;
	}

	// Update scroll panel border and scrollbar colors (once per update, not per button)
	int r, g, b, a = 0;
	UnpackRGB(r, g, b, HudColor());
	if (m_pScrollPanelBorder)
	{
		m_pScrollPanelBorder->setLineColor(r, g, b, 255);
		m_pScrollPanel->setBorder(m_pScrollPanelBorder);
	}
	pTitleLabel->setFgColor(r, g, b, 0);
	
	// Update scheme colors for scrollbar components
	Scheme* pScheme = App::getInstance()->getScheme();
	pScheme->setColor(Scheme::sc_primary1, r, g, b, a);
	pScheme->setColor(Scheme::sc_primary2, r, g, b, a);
	pScheme->setColor(Scheme::sc_secondary1, r, g, b, a);
	
	// Force scrollbars to repaint with new colors
	ScrollBar* pVerticalScrollBar = m_pScrollPanel->getVerticalScrollBar();
	ScrollBar* pHorizontalScrollBar = m_pScrollPanel->getHorizontalScrollBar();
	if (pVerticalScrollBar)
		pVerticalScrollBar->repaint();
	if (pHorizontalScrollBar)
		pHorizontalScrollBar->repaint();

	for ( int i = 0; i < MAX_MUTATORS_CL; i++ )
	{
		if (m_pButtons[i])
		{
			// Update button text (without vote count)
			char sz[64];
			sprintf(sz, " %s", sMutators[i].name);
			m_pButtons[i]->setText(sz);
			
			// Update vote tally label
			if (m_pVoteTallyLabels[i])
			{
				char voteSz[16];
				sprintf(voteSz, "%d", votes[i]);
				m_pVoteTallyLabels[i]->setText(voteSz);
				m_pVoteTallyLabels[i]->setFgColor(r, g, b, 0);
			}

			Color borderColor;
			if ((myVote - 1) == i)
			{
				m_pButtons[i]->setArmed(true);
				borderColor = Color( 255, 255, 255, a );
				m_pVoteTallyLabels[i]->setFgColor(255, 255, 255, 0);
			}
			else
			{
				// Update vote tally color to match button state
				if ((i == hi || i == s || i == t) && votes[i] > 0)
				{
					borderColor = Color( 0, 255, 0, a );
				}
				else
				{
					borderColor = Color( r, g, b, a );
				}
			}
			if ( m_pButtonBorders[i] )
			{
				int br, bg, bb, ba;
				borderColor.getColor(br, bg, bb, ba);
				m_pButtonBorders[i]->setLineColor(br, bg, bb, ba);
				m_pButtons[i]->setBorder( m_pButtonBorders[i] );
			}


			m_pButtons[i]->setUnArmedColor(r, g, b, 0);
			if (votes[i] > 0)
			{
				m_pButtons[i]->setArmed(true);
				m_pButtons[i]->setBgColor(r, g, b, 255);
				if ((myVote - 1) == i)
					m_pButtons[i]->setArmedColor(255, 255, 255, 0);
				else 
					m_pButtons[i]->setArmedColor(r, g, b, 0);
			}
			else
			{
				if ( m_pButtonBorders[i] )
				{
					m_pButtonBorders[i]->setLineColor(r, g, b, a);
					m_pButtons[i]->setBorder( m_pButtonBorders[i] );
				}
			}
		}
	}

	for ( int k = 0; k < MUTATOR_MENU_SPECIAL_SLOTS; k++ )
	{
		ColorButton *pButton = m_pSpecialButtons[k];
		LineBorder *pBorder = m_pSpecialButtonBorders[k];
		Label *pTally = m_pSpecialVoteTallyLabels[k];
		if ( !pButton )
			continue;

		int voteId = s_MutatorSpecialSlots[k].voteId;
		int slot = voteId - 1;

		if ( pTally )
		{
			char voteSz[16];
			sprintf(voteSz, "%d", votes[slot]);
			pTally->setText(voteSz);
			pTally->setFgColor(r, g, b, 0);
		}

		Color borderColor;
		if ( myVote == voteId )
		{
			pButton->setArmed(true);
			borderColor = Color( 255, 255, 255, a );
			if ( pTally )
				pTally->setFgColor(255, 255, 255, 0);
		}
		else if ((slot == hi || slot == s || slot == t) && votes[slot] > 0)
		{
			borderColor = Color( 0, 255, 0, a );
		}
		else
		{
			borderColor = Color( r, g, b, a );
		}

		if ( pBorder )
		{
			int br, bg, bb, ba;
			borderColor.getColor(br, bg, bb, ba);
			pBorder->setLineColor(br, bg, bb, ba);
			pButton->setBorder( pBorder );
		}

		pButton->setUnArmedColor(r, g, b, 0);
		if ( votes[slot] > 0 )
		{
			pButton->setArmed(true);
			pButton->setBgColor(r, g, b, 255);
			if ( myVote == voteId )
				pButton->setArmedColor(255, 255, 255, 0);
			else
				pButton->setArmedColor(r, g, b, 0);
		}
		else if ( pBorder )
		{
			pBorder->setLineColor(r, g, b, a);
			pButton->setBorder( pBorder );
		}
	}

	const char *myVoteName = "None";
	if ( myVote == MUTATOR_VOTE_INSTANT )
		myVoteName = "INSTANT";
	else if ( myVote == MUTATOR_VOTE_CLEAR )
		myVoteName = "CLEAR";
	else if ( myVote > 0 && myVote <= MAX_MUTATORS_CL )
		myVoteName = sMutators[myVote - 1].name;

	pTitleLabel->setText("%s for %s | Your Vote: %s | Time Left: %.1f\n",
		gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMutator"),
		g_szMutatorVoteModeClient,
		myVoteName, seconds);
}

//======================================
// Key inputs for the Class Menu
bool CVoteMutatorPanel::SlotInput( int iSlot )
{
	if ( (iSlot < 0) || (iSlot > 9) )
		return false;

	if ( !m_pButtons[ iSlot ] )
		return false;

	// Do nothing.
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
	if ( iInput > (MAX_MUTATORS_CL - 1) || iInput < 0 )
		iInput = 0;

	m_iCurrentInfo = iInput;
}
