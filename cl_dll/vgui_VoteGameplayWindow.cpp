
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
#define CLASSMENU_BUTTON_SIZE_X			XRES(128)
#define CLASSMENU_BUTTON_SIZE_Y			YRES(24)
#define CLASSMENU_BUTTON_SPACER_Y		YRES(8)
#define CLASSMENU_WINDOW_X				XRES(176)
#define CLASSMENU_WINDOW_Y				YRES(80)
#define CLASSMENU_WINDOW_SIZE_X			XRES(420)
#define CLASSMENU_WINDOW_SIZE_Y			YRES(312)
#define CLASSMENU_WINDOW_TEXT_X			XRES(150)
#define CLASSMENU_WINDOW_TEXT_Y			YRES(80)
#define CLASSMENU_WINDOW_NAME_X			XRES(150)
#define CLASSMENU_WINDOW_NAME_Y			YRES(8)

CVoteGameplayPanel::CVoteGameplayPanel(int iTrans, int iRemoveMe, int x,int y,int wide,int tall) : CMenuPanel(iTrans, iRemoveMe, x,y,wide,tall)
{
	// don't show graphics atm
	bool bShowClassGraphic = false;

	memset( m_pClassImages, 0, sizeof(m_pClassImages) );

	// Get the scheme used for the Titles
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	// schemes
	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hPlayWindowText = pSchemes->getSchemeHandle( "Scoreboard Title Text" );
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
	pTitleLabel->setText(gHUD.m_TextMessage.BufferedLocaliseTextString("#Title_VoteGameplay"));

	// Create the Scroll panel
	m_pScrollPanel = new CTFScrollPanel( CLASSMENU_WINDOW_X, CLASSMENU_WINDOW_Y, CLASSMENU_WINDOW_SIZE_X, CLASSMENU_WINDOW_SIZE_Y );
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
		int iYPos = CLASSMENU_TOPLEFT_BUTTON_Y + ( (CLASSMENU_BUTTON_SIZE_Y + CLASSMENU_BUTTON_SPACER_Y) * i );
		char voteCommand[16];
		sprintf(voteCommand, "vote %d", i+1);
		ActionSignal *pASignal = new CMenuHandler_StringCommandClassSelect(voteCommand, false );

		// Class button
		sprintf(sz, " %s", CHudTextMessage::BufferedLocaliseTextString( sLocalisedGameplayModes[i] ) );
		m_pButtons[i] = new ClassButton( i, sz, CLASSMENU_TOPLEFT_BUTTON_X, iYPos, CLASSMENU_BUTTON_SIZE_X, CLASSMENU_BUTTON_SIZE_Y, true);
		m_pButtons[i]->setBoundKey( (char)255 );
		m_pButtons[i]->setContentAlignment( vgui::Label::a_west );
		m_pButtons[i]->addActionSignal( pASignal );
		m_pButtons[i]->addInputSignal( new CHandler_MenuButtonOver(this, i) );
		m_pButtons[i]->setParent( this );

		// Create the Class Info Window
		//m_pClassInfoPanel[i] = new CTransparentPanel( 255, CLASSMENU_WINDOW_X, CLASSMENU_WINDOW_Y, CLASSMENU_WINDOW_SIZE_X, CLASSMENU_WINDOW_SIZE_Y );
		m_pClassInfoPanel[i] = new CTransparentPanel( 255, 0, 0, clientWide, CLASSMENU_WINDOW_SIZE_Y );
		m_pClassInfoPanel[i]->setParent( m_pScrollPanel->getClient() );
		//m_pClassInfoPanel[i]->setVisible( false );

		// don't show class pic in lower resolutions
		int textOffs = XRES(8);

		if ( bShowClassGraphic )
		{
			textOffs = CLASSMENU_WINDOW_NAME_X;
		}

		// Create the Gameplay Name Label
		sprintf(sz, "%s", sLocalisedGameplayModes[i]);
		char* localName=CHudTextMessage::BufferedLocaliseTextString( sz );
		Label *pNameLabel = new Label( "", textOffs, CLASSMENU_WINDOW_NAME_Y );
		pNameLabel->setFont( pSchemes->getFont(hTitleScheme) ); 
		pNameLabel->setParent( m_pClassInfoPanel[i] );
		pSchemes->getFgColor( hTitleScheme, r, g, b, a );
		pNameLabel->setFgColor( r, g, b, a );
		pSchemes->getBgColor( hTitleScheme, r, g, b, a );
		pNameLabel->setBgColor( r, g, b, a );
		pNameLabel->setContentAlignment( vgui::Label::a_west );
		pNameLabel->setBorder(new LineBorder());
		pNameLabel->setText( "%s", localName);

		// Create the Class Image
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

				m_pClassImages[team][i] = new CImageLabel( sz, 0, 0, CLASSMENU_WINDOW_TEXT_X, CLASSMENU_WINDOW_TEXT_Y );

				CImageLabel *pLabel = m_pClassImages[team][i];
				pLabel->setParent( m_pClassInfoPanel[i] );
				//pLabel->setBorder(new LineBorder());

				if ( team != 1 )
				{
					pLabel->setVisible( false );
				}
				
				// Reposition it based upon it's size
				int xOut, yOut;
				pNameLabel->getTextSize( xOut, yOut );
				pLabel->setPos( (CLASSMENU_WINDOW_TEXT_X - pLabel->getWide()) / 2, yOut /2 );
			}
		}

		// Open up the Class Briefing File
		sprintf(sz, "modes/%s.txt", sGameplayModes[i]);
		char *cText = "Gamemode description not available.";
		char *pfile = (char *)gEngfuncs.COM_LoadFile( sz, 5, NULL );
		if (pfile)
		{
			cText = pfile;
		}
		
		// Create the Text info window
		TextPanel *pTextWindow = new TextPanel(cText, textOffs, YRES(40), (CLASSMENU_WINDOW_SIZE_X - textOffs)-5, CLASSMENU_WINDOW_SIZE_Y - CLASSMENU_WINDOW_TEXT_Y);
		pTextWindow->setParent( m_pClassInfoPanel[i] );
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
		if (m_pClassImages[0][i]!=null)
		{
			m_pClassImages[0][i]->getPos(xx,yy);
			if ((yy+m_pClassImages[0][i]->getTall())>maxY)
			{
				maxY=yy+m_pClassImages[0][i]->getTall();
			}
		}

		m_pClassInfoPanel[i]->setSize( maxX , maxY );
		if (pfile) gEngfuncs.COM_FreeFile( pfile );
		//m_pClassInfoPanel[i]->setBorder(new LineBorder());

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
			m_pClassInfoPanel[i]->setVisible( false );
		}

		m_pButtons[ iSlot ]->setArmed( true );
		m_pClassInfoPanel[ iSlot ]->setVisible( true );
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
		m_pClassInfoPanel[i]->setVisible( false );
	}

	if ( iInput > MAX_MODES || iInput < 0 )
		iInput = 0;

	//m_pButtons[iInput]->setArmed( true );
	m_pClassInfoPanel[iInput]->setVisible( true );
	m_iCurrentInfo = iInput;

	m_pScrollPanel->setScrollValue(0,0);
	m_pScrollPanel->validate();
}
