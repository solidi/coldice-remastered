
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
#define MUTATORMENU_TOPLEFT_BUTTON_X		XRES(40)
#define MUTATORMENU_TOPLEFT_BUTTON_Y		YRES(80)
#define MUTATORMENU_BUTTON_SIZE_X			XRES(104)
#define MUTATORMENU_BUTTON_SIZE_Y			YRES(24)
#define MUTATORMENU_BUTTON_SPACER_Y		YRES(8)

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

	// Create the buttons
	int positionCount = 0;
	for (int i = 0; i < MAX_MUTATORS; i++)
	{
		m_pButtons[i] = NULL;
		if (strstr(sMutators[i], "slowmo") ||
			strstr(sMutators[i], "speedup") ||
			strstr(sMutators[i], "topsyturvy") ||
			strstr(sMutators[i], "explosiveai"))
		{
			continue;
		}

		// Space for random button
		int xI = positionCount+1;
		int degree = (positionCount+1) / 12;
		if (i == MAX_MUTATORS - 1)
		{
			xI = 0;
			degree = 0;
		}
		char sz[256];
		int iYPos = MUTATORMENU_TOPLEFT_BUTTON_Y + ( (MUTATORMENU_BUTTON_SIZE_Y + MUTATORMENU_BUTTON_SPACER_Y) * xI );
		int spacer = 0;
		spacer = (MUTATORMENU_BUTTON_SIZE_X + 10) * degree;
		iYPos = MUTATORMENU_TOPLEFT_BUTTON_Y + ( (MUTATORMENU_BUTTON_SIZE_Y + MUTATORMENU_BUTTON_SPACER_Y) * (xI - (12 * degree)));
		
		char voteCommand[16];
		sprintf(voteCommand, "vote %d", i+1);
		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect(voteCommand, false );

		// mutator buttons
		sprintf(sz, " %s", sMutators[i]);
		m_pButtons[i] = new ClassButton( i, sz, MUTATORMENU_TOPLEFT_BUTTON_X + spacer, iYPos, MUTATORMENU_BUTTON_SIZE_X, MUTATORMENU_BUTTON_SIZE_Y, true);
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setParent(this);
		positionCount++;
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
	{
		if (m_pButtons[j])
			m_pButtons[j]->setArmed(false);
	}

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

		if (m_pButtons[i])
		{
			char sz[64];
			sprintf(sz, " %-2d %s", votes[i], sMutators[i]);
			m_pButtons[i]->setText(sz);
		}

		if ((myVote - 1) == i)
		{
			if (m_pButtons[i])
				m_pButtons[i]->setArmed(true);
		}
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
