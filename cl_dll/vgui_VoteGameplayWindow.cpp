
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

// Game Menu Dimensions
#define GAMEMENU_TITLE_X				XRES(40)
#define GAMEMENU_TITLE_Y				YRES(32)
#define GAMEMENU_TOPLEFT_BUTTON_X		XRES(40)
#define GAMEMENU_TOPLEFT_BUTTON_Y		YRES(80)
#define GAMEMENU_BUTTON_SIZE_X			XRES(128)
#define GAMEMENU_BUTTON_SIZE_Y			YRES(42)
#define GAMEMENU_BUTTON_SPACER_Y		YRES(8)
#define GAMEMENU_WINDOW_X				XRES(176)
#define GAMEMENU_WINDOW_Y				YRES(80)
#define GAME2MENU_TOPLEFT_BUTTON_X		XRES(480)
#define GAMEMENU_WINDOW_SIZE_X			XRES(292)
#define GAMEMENU_WINDOW_SIZE_Y			YRES(342)
#define GAMEMENU_WINDOW_TEXT_X			XRES(150)
#define GAMEMENU_WINDOW_TEXT_Y			YRES(80)
#define GAMEMENU_WINDOW_NAME_X			XRES(150)
#define GAMEMENU_WINDOW_NAME_Y			YRES(8)

CVoteGameplayPanel::CVoteGameplayPanel(int iTrans, int iRemoveMe, int x,int y,int wide,int tall) : CMenuPanel(iTrans, iRemoveMe, x,y,wide,tall)
{
	// don't show graphics atm
	bool bShowClassGraphic = false;

	//memset( m_pClassImages, 0, sizeof(m_pClassImages) );

	// Get the scheme used for the Titles
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	// schemes
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hPlayWindowText = pSchemes->getSchemeHandle( "Scoreboard Title Text" );
	SchemeHandle_t hClassWindowText = pSchemes->getSchemeHandle( "Briefing Text" );

	// color schemes
	int r, g, b, a;

	// Create the title
	pTitleLabel = new Label( "", GAMEMENU_TITLE_X, GAMEMENU_TITLE_Y );
	pTitleLabel->setParent( this );
	pTitleLabel->setFont( pSchemes->getFont(hTitleScheme) );
	pSchemes->getFgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	pTitleLabel->setBgColor( r, g, b, a );
	pTitleLabel->setContentAlignment( vgui::Label::a_west );
	pTitleLabel->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteGameplay"));

	// Create the Scroll panel
	m_pScrollPanel = new CTFScrollPanel( GAMEMENU_WINDOW_X, GAMEMENU_WINDOW_Y, GAMEMENU_WINDOW_SIZE_X, GAMEMENU_WINDOW_SIZE_Y );
	m_pScrollPanel->setParent(this);
	//force the scrollbars on, so after the validate clientClip will be smaller
	m_pScrollPanel->setScrollBarAutoVisible(false, false);
	m_pScrollPanel->setScrollBarVisible(true, true);
	UnpackRGB(r, g, b, HudColor());
	m_pScrollPanel->setBorder( new LineBorder( Color(r, g, b, 0) ) );
	m_pScrollPanel->validate();

	int clientWide=m_pScrollPanel->getClient()->getWide();

	//turn scrollpanel back into auto show scrollbar mode and validate
	m_pScrollPanel->setScrollBarAutoVisible(false,true);
	m_pScrollPanel->setScrollBarVisible(false,false);
	m_pScrollPanel->validate();

	// Create the gameplay buttons
	for (int i = 0; i <= MAX_MODES; i++)
	{
		char sz[256];
		int shift = i+1;
		int iXPos = GAMEMENU_TOPLEFT_BUTTON_X;

		// Half way
		if (i >= ((MAX_MODES) / 2))
		{
			shift -= ((MAX_MODES) / 2) + 1;
			iXPos = GAME2MENU_TOPLEFT_BUTTON_X;
		}

		// Random button
		if (i == MAX_MODES)
		{
			shift = 0;
			iXPos = GAMEMENU_TOPLEFT_BUTTON_X;
		}

		int iYPos = GAMEMENU_TOPLEFT_BUTTON_Y + ( (GAMEMENU_BUTTON_SIZE_Y + GAMEMENU_BUTTON_SPACER_Y) * shift );
		char voteCommand[16];
		sprintf(voteCommand, "vote %d", i+1);
		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect(voteCommand, false );

		// gameplay button
		sprintf(sz, " %s", CHudTextMessage::BufferedLocaliseTextString( sLocalisedGameplayModes[i] ) );
		m_pButtons[i] = new ClassButton( i, sz, iXPos, iYPos, GAMEMENU_BUTTON_SIZE_X, GAMEMENU_BUTTON_SIZE_Y, true);
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setParent( this );

		// Create the game Info Window
		m_pGameInfoPanel[i] = new CTransparentPanel( 255, 0, 0, clientWide, GAMEMENU_WINDOW_SIZE_Y );
		m_pGameInfoPanel[i]->setParent( m_pScrollPanel->getClient() );

		// don't show class pic in lower resolutions
		int textOffs = XRES(8);

		if ( bShowClassGraphic )
		{
			textOffs = GAMEMENU_WINDOW_NAME_X;
		}

		// Create the Gameplay Name Label
		sprintf(sz, "%s", sLocalisedGameplayModes[i]);
		char* localName = CHudTextMessage::BufferedLocaliseTextString( sz );
		Label *pNameLabel = new Label( "", textOffs, GAMEMENU_WINDOW_NAME_Y );
		pNameLabel->setFont( pSchemes->getFont(hTitleScheme) ); 
		pNameLabel->setParent( m_pGameInfoPanel[i] );
		pSchemes->getFgColor( hTitleScheme, r, g, b, a );
		pNameLabel->setFgColor( r, g, b, a );
		pSchemes->getBgColor( hTitleScheme, r, g, b, a );
		pNameLabel->setBgColor( r, g, b, a );
		pNameLabel->setContentAlignment( vgui::Label::a_west );
		pNameLabel->setBorder(new LineBorder());
		pNameLabel->setText( "%s", localName);

		// Create the Class Image
		/*
		if ( bShowClassGraphic )
		{
			for ( int team = 0; team < 2; team++ )
			{
				if ( team == 1 )
				{
					sprintf( sz, "%sred", sTFClassSelection[i] );
				}
				else
				{
					sprintf( sz, "%sblue", sTFClassSelection[i] );
				}

				m_pClassImages[team][i] = new CImageLabel( sz, 0, 0, GAMEMENU_WINDOW_TEXT_X, GAMEMENU_WINDOW_TEXT_Y );

				CImageLabel *pLabel = m_pClassImages[team][i];
				pLabel->setParent( m_pGameInfoPanel[i] );
				//pLabel->setBorder(new LineBorder());

				if ( team != 1 )
				{
					pLabel->setVisible( false );
				}
				
				// Reposition it based upon it's size
				int xOut, yOut;
				pNameLabel->getTextSize( xOut, yOut );
				pLabel->setPos( (GAMEMENU_WINDOW_TEXT_X - pLabel->getWide()) / 2, yOut /2 );
			}
		}
		*/

		// Open up the game Briefing File
		sprintf(sz, "modes/%s.txt", sGameplayModes[i]);
		char *cText = "Gamemode description not available.";
		char *pfile = (char *)gEngfuncs.COM_LoadFile( sz, 5, NULL );
		if (pfile)
		{
			cText = pfile;
		}
		
		// Create the Text info window
		TextPanel *pTextWindow = new TextPanel(cText, textOffs, YRES(40), (GAMEMENU_WINDOW_SIZE_X - textOffs)-5, GAMEMENU_WINDOW_SIZE_Y - GAMEMENU_WINDOW_TEXT_Y);
		pTextWindow->setParent( m_pGameInfoPanel[i] );
		pTextWindow->setFont( pSchemes->getFont(hPlayWindowText) );
		pSchemes->getFgColor( hClassWindowText, r, g, b, a );
		pTextWindow->setFgColor( r, g, b, a );
		pSchemes->getBgColor( hClassWindowText, r, g, b, a );
		pTextWindow->setBgColor( r, g, b, a );

		// Resize the Info panel to fit it all
		int wide,tall;
		pTextWindow->getTextImage()->getTextSizeWrapped( wide,tall);
		pTextWindow->setSize(wide,tall);

		int xx,yy;
		pTextWindow->getPos(xx,yy);
		int maxX=xx+wide;
		int maxY=yy+tall;

		//check to see if the image goes lower than the text
		//just use the red teams [0] images
		/*
		if (m_pClassImages[0][i]!=null)
		{
			m_pClassImages[0][i]->getPos(xx,yy);
			if ((yy+m_pClassImages[0][i]->getTall())>maxY)
			{
				maxY=yy+m_pClassImages[0][i]->getTall();
			}
		}
		*/

		m_pGameInfoPanel[i]->setSize( maxX , maxY );
		if (pfile) gEngfuncs.COM_FreeFile( pfile );
	}

	m_iCurrentInfo = 0;
}

// Update
void CVoteGameplayPanel::Update()
{
	// Time
	float minutes = fmax( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) / 60);
	float seconds = fmax( 0, ( m_iTime + m_fStartTime - gHUD.m_flTime ) - (minutes * 60));

	int votes[MAX_MODES + 1];
	int myVote = -1;

	// Count votes
	for (int j = 0; j <= MAX_MODES; j++)
		m_pButtons[j]->setArmed(false);

	for ( int i = 0; i <= MAX_MODES; i++ )
	{
		votes[i] = 0;
		for ( int j = 1; j <= MAX_PLAYERS; j++ )
		{
			//if ( g_PlayerInfoList[j].name == NULL )
			//	continue; // empty player slot, skip
			if ( g_PlayerInfoList[j].thisplayer )
				myVote = g_PlayerExtraInfo[j].vote;

			if (g_PlayerExtraInfo[j].vote == (i + 1))
				votes[i] += 1;
		}

		char sz[256];
		sprintf(sz, " %-2d %s", votes[i], CHudTextMessage::BufferedLocaliseTextString( sLocalisedGameplayModes[i] ));
		m_pButtons[i]->setText(sz);

		if ((myVote - 1) == i)
			m_pButtons[i]->setArmed(true);
	}

	pTitleLabel->setText("%s | Your Vote: %s | Time Left: %.1f\n", gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteGameplay"), myVote > 0 ? sGameplayModes[myVote-1] : "None", seconds);
}

//======================================
// Key inputs for the Class Menu
bool CVoteGameplayPanel::SlotInput( int iSlot )
{
	if ( (iSlot < 0) || (iSlot > 9) )
		return false;

	if ( !m_pButtons[ iSlot ] )
		return false;

	//if ( !(m_pButtons[ iSlot ]->IsNotValid()) )
	{
		for (int i = 0; i <= MAX_MODES; i++)
		{
			m_pButtons[i]->setArmed( false );
			m_pGameInfoPanel[i]->setVisible( false );
		}

		m_pButtons[ iSlot ]->setArmed( true );
		m_pGameInfoPanel[ iSlot ]->setVisible( true );
		m_iCurrentInfo = iSlot;
		m_pButtons[ iSlot ]->fireActionSignal();
		return true;
	}

	return false;
}

//======================================
// Update the Class menu before opening it
void CVoteGameplayPanel::Open( void )
{
	SetActiveInfo(0);
	Update();
	CMenuPanel::Open();
}

//-----------------------------------------------------------------------------
// Purpose: Called each time a new level is started.
//-----------------------------------------------------------------------------
void CVoteGameplayPanel::Initialize( void )
{
	setVisible( false );
	m_pScrollPanel->setScrollValue( 0, 0 );
}

//======================================
// Mouse is over a class button, bring up the class info
void CVoteGameplayPanel::SetActiveInfo( int iInput )
{
	// Remove all the Info panels and bring up the specified one
	for (int i = 0; i <= MAX_MODES; i++)
	{
		//m_pButtons[i]->setArmed( false );
		m_pGameInfoPanel[i]->setVisible( false );
	}

	if ( iInput > MAX_MODES || iInput < 0 )
		iInput = 0;

	//m_pButtons[iInput]->setArmed( true );
	m_pGameInfoPanel[iInput]->setVisible( true );
	m_iCurrentInfo = iInput;

	m_pScrollPanel->setScrollValue(0,0);
	m_pScrollPanel->validate();
}
