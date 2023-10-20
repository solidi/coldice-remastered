
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

// Class Menu Dimensions
#define CLASSMENU_TITLE_X				XRES(40)
#define CLASSMENU_TITLE_Y				YRES(32)
#define CLASSMENU_TOPLEFT_BUTTON_X		XRES(40)
#define CLASSMENU_TOPLEFT_BUTTON_Y		YRES(80)
#define CLASSMENU_BUTTON_SIZE_X			XRES(124)
#define CLASSMENU_BUTTON_SIZE_Y			YRES(24)
#define CLASSMENU_BUTTON_SPACER_Y		YRES(8)
#define CLASSMENU_WINDOW_X				XRES(176)
#define CLASSMENU_WINDOW_Y				YRES(80)
#define CLASSMENU_WINDOW_SIZE_X			XRES(424)
#define CLASSMENU_WINDOW_SIZE_Y			YRES(312)
#define CLASSMENU_WINDOW_TEXT_X			XRES(150)
#define CLASSMENU_WINDOW_TEXT_Y			YRES(80)
#define CLASSMENU_WINDOW_NAME_X			XRES(150)
#define CLASSMENU_WINDOW_NAME_Y			YRES(8)
#define CLASSMENU_WINDOW_PLAYERS_Y		YRES(42)

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
	pTitleLabel = new Label( "", CLASSMENU_TITLE_X, CLASSMENU_TITLE_Y );
	pTitleLabel->setParent( this );
	pTitleLabel->setFont( pSchemes->getFont(hTitleScheme) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setBgColor( r, g, b, a );
	pTitleLabel->setContentAlignment( vgui::Label::a_west );
	pTitleLabel->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMutator"));

	// Create the buttons
	for (int i = 0; i < MAX_MUTATORS; i++)
	{
		int degree = i / 12;
		char sz[256]; 
		int iYPos = CLASSMENU_TOPLEFT_BUTTON_Y + ( (CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y) * i );
		int spacer = 0;
		//if (i > 11)
		{
			spacer = (CLASSMENU_BUTTON_SIZE_X + 10) * degree;
			iYPos = CLASSMENU_TOPLEFT_BUTTON_Y + ( (CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y) * (i - (12 * degree)));
		}
		
		char voteCommand[64];
		sprintf(voteCommand, "vote %d", i+1);
		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect(voteCommand, false );

		// Class button
		sprintf(sz, " %s", sMutators[i]);
		m_pButtons[i] = new ClassButton( i, sz, CLASSMENU_TOPLEFT_BUTTON_X + spacer, iYPos, CLASSMENU_BUTTON_SIZE_X, CLASSMENU_BUTTON_SIZE_Y, true);
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setParent(this);
	}

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
		m_pButtons[j]->setArmed(false);

	// Count votes
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

		char sz[64];
		sprintf(sz, " %-2d %s", votes[i], sMutators[i]);
		m_pButtons[i]->setText(sz);

		if ((myVote - 1) == i)
			m_pButtons[i]->setArmed(true);
	}

	pTitleLabel->setText("%s | Your Vote: %s | Time Left: %.1f\n",
		gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteMutator"),
		myVote > 0 ? sMutators[myVote-1] : "None", seconds);
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
