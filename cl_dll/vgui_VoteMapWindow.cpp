
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

// Map Menu Dimensions
#define MAPMENU_SCROLL_X			XRES(80)
#define MAPMENU_SCROLL_Y			YRES(80)
#define MAPMENU_TITLE_X				XRES(40)
#define MAPMENU_TITLE_Y				YRES(32)
#define MAPMENU_TOPLEFT_BUTTON_X		XRES(40)
#define MAPMENU_TOPLEFT_BUTTON_Y		YRES(80)
#define MAPMENU_BUTTON_SIZE_X			XRES(124)
#define MAPMENU_BUTTON_SIZE_Y			YRES(22)
#define MAPMENU_BUTTON_SPACER_Y		YRES(8)
#define MAPMENU_ITEMS_PER_COL		13

// Creation
CVoteMapPanel::CVoteMapPanel(int iTrans, int iRemoveMe, int x,int y,int wide,int tall) : CMenuPanel(iTrans, iRemoveMe, x,y,wide,tall)
{
	// Get the scheme used for the Titles
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	// schemes
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );

	// color schemes
	int r, g, b, a;

	// Create the title
	pTitleLabel = new Label( "", MAPMENU_TITLE_X, MAPMENU_TITLE_Y );
	pTitleLabel->setParent( this );
	pTitleLabel->setFont( pSchemes->getFont(hTitleScheme) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setBgColor( r, g, b, a );
	pTitleLabel->setContentAlignment( vgui::Label::a_west );
	pTitleLabel->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMap"));

	// Create the scroll panel
	m_pScrollPanel = new CTFScrollPanel( MAPMENU_SCROLL_X, MAPMENU_SCROLL_Y, XRES(480), YRES(340) );
	m_pScrollPanel->setParent(this);
	m_pScrollPanel->setScrollBarAutoVisible(true, true);
	m_pScrollPanel->setScrollBarVisible(false, true);
	m_pScrollPanelBorder = new LineBorder( Color(r, g, b, 255) );
	m_pScrollPanel->setBorder( m_pScrollPanelBorder );
	m_pScrollPanel->validate();

	// Create the map buttons inside scroll panel
	int positionCount = 0;
	for (int i = 0; i < BUILT_IN_MAP_COUNT; i++)
	{
		// Calculate position in grid layout (2 columns)
		int column, row;
		
		// Special handling for "Random" button (last map) - always first position
		if (i == BUILT_IN_MAP_COUNT - 1)
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
		
		int iXPos = XRES(8) + (column * (XRES(222) + XRES(8)));
		int iYPos = YRES(4) + (row * (YRES(36) + YRES(4)));
		
		char voteCommand[16];
		sprintf(voteCommand, "vote %d", i+1);
		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect(voteCommand, false );

		// Map button
		char sz[256];
		sprintf(sz, " %s", sBuiltInMaps[i]);
		m_pButtons[i] = new ColorButton( sz, iXPos, iYPos, XRES(222), YRES(36), false, true);
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setFont( pSchemes->getFont(hTitleScheme) );
		m_pButtons[i]->setParent( m_pScrollPanel->getClient() );
		
		// Add vote tally label (right-aligned, vertically centered)
		m_pVoteTallyLabels[i] = new Label( "0", XRES(222) - XRES(25), YRES(10) );
		m_pVoteTallyLabels[i]->setParent( m_pButtons[i] );
		m_pVoteTallyLabels[i]->setFont( pSchemes->getFont(hTitleScheme) );
		m_pVoteTallyLabels[i]->setContentAlignment( vgui::Label::a_east );
		m_pVoteTallyLabels[i]->setSize( XRES(10), YRES(18) );
		pSchemes->getFgColor( hTitleScheme, r, g, b, a );
		m_pVoteTallyLabels[i]->setFgColor( r, g, b, a );
		m_pVoteTallyLabels[i]->setBgColor( 0, 0, 0, 255 );
	}
	
	// Validate scroll panel after adding all buttons
	m_pScrollPanel->validate();

	m_iCurrentInfo = 0;
}

void CVoteMapPanel::Update()
{
	// Time
	float minutes = fmax( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) / 60);
	float seconds = fmax( 0, ( m_iTime + m_fStartTime - gHUD.m_flTime ) - (minutes * 60));

	int votes[BUILT_IN_MAP_COUNT];
	int myVote = -1;

	// Count votes
	int highest = -1, hi = -1;
	for (int j = 0; j < BUILT_IN_MAP_COUNT; j++)
		m_pButtons[j]->setArmed(false);

	// Count votes
	for ( int i = 0; i < BUILT_IN_MAP_COUNT; i++ )
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

	// Update scroll panel border and scrollbar colors (once per update, not per button)
	int r, g, b, a = 0;
	UnpackRGB(r, g, b, HudColor());
	if (m_pScrollPanelBorder)
	{
		m_pScrollPanelBorder->setColor(Color(r, g, b, 255));
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

	for ( int i = 0; i < BUILT_IN_MAP_COUNT; i++ )
	{
		if (m_pButtons[i])
		{
			// Update button text (without vote count)
			char sz[64];
			sprintf(sz, " %s", sBuiltInMaps[i]);
			m_pButtons[i]->setText(sz);
			
			// Update vote tally label
			if (m_pVoteTallyLabels[i])
			{
				char voteSz[16];
				sprintf(voteSz, "%d", votes[i]);
				m_pVoteTallyLabels[i]->setText(voteSz);
				
				// Update vote tally color to match button state
				if (votes[i] == highest)
				{
					m_pVoteTallyLabels[i]->setFgColor(255, 255, 255, 0);
				}
				else
				{
					m_pVoteTallyLabels[i]->setFgColor(r, g, b, 0);
				}
			}

			if ((myVote - 1) == i)
				m_pButtons[i]->setArmed(true);

			m_pButtons[i]->setUnArmedColor(r, g, b, 0);
			if (votes[i] > 0)
			{
				m_pButtons[i]->setArmed(true);
				if (votes[i] == highest)
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

	pTitleLabel->setText("%s | Your Vote: %s | Time Left: %.1f\n", gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMap"), myVote > 0 ? sBuiltInMaps[myVote-1] : "None", seconds);
}

//======================================
// Key inputs for the Class Menu
bool CVoteMapPanel::SlotInput( int iSlot )
{
	if ( (iSlot < 0) || (iSlot > 9) )
		return false;

	if ( !m_pButtons[ iSlot ] )
		return false;

	//if ( !(m_pButtons[ iSlot ]->IsNotValid()) )
	{
		for (int i = 0; i < BUILT_IN_MAP_COUNT; i++)
		{
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
void CVoteMapPanel::Open( void )
{
	SetActiveInfo(0);
	Update();
	CMenuPanel::Open();
}

//-----------------------------------------------------------------------------
// Purpose: Called each time a new level is started.
//-----------------------------------------------------------------------------
void CVoteMapPanel::Initialize( void )
{
	setVisible( false );
}

//======================================
// Mouse is over a class button, bring up the class info
void CVoteMapPanel::SetActiveInfo( int iInput )
{
	if ( iInput > (BUILT_IN_MAP_COUNT - 1) || iInput < 0 )
		iInput = 0;

	m_iCurrentInfo = iInput;
}
