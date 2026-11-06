//
// awards_shared.h
//
// Multiplayer end-of-match style awards modeled after GoldenEye 007 (N64).
// Shared by the server (dlls/awards.cpp) and the client scoreboards
// (cl_dll/scoreboard.cpp, cl_dll/vgui_ScorePanel.cpp). See ai/award_system.md.
//

#ifndef AWARDS_SHARED_H
#define AWARDS_SHARED_H

// Award indices are wire-visible: the client resolves names from this order.
// 0 means "no award". Order follows the N64 g_AwardNames[] table.
enum award_e
{
	AWARD_NONE = 0,
	AWARD_LEMMING,				// most suicides
	AWARD_WHERES_THE_AMMO,		// fewest shots fired (under 100)
	AWARD_WHERES_THE_ARMOR,		// fewest armor pickups (2 or less)
	AWARD_AC10,					// most armor pickups (more than 6)
	AWARD_MARKSMANSHIP,			// most headshots
	AWARD_MOST_PROFESSIONAL,	// best kills per shot
	AWARD_MOST_DEADLY,			// best kill / death ratio
	AWARD_MOSTLY_HARMLESS,		// worst kill / death ratio
	AWARD_MOST_COWARDLY,		// least time with other players in view
	AWARD_MOST_FRANTIC,			// fastest average speed (over 10 km/h)
	AWARD_MOST_HONORABLE,		// fewest hits landed on an opponent's back
	AWARD_MOST_DISHONORABLE,	// most hits landed on an opponent's back
	AWARD_SHORTEST_INNINGS,		// shortest life
	AWARD_LONGEST_INNINGS,		// longest life
	AWARD_DOUBLE_KILL,			// 2 kills within the multi-kill window
	AWARD_TRIPLE_KILL,			// 3 kills within the multi-kill window
	AWARD_QUAD_KILL,			// 4+ kills within the multi-kill window
	AWARD_COUNT
};

// "Awards" user message payload: one 16-bit short.
//   bits 15..10  weapon of choice (WEAPON_* id, 0 = none)
//   bits  9..5   first award (award_e)
//   bits  4..0   second award (award_e)
#define AWARDS_WEAPON_BITS		6
#define AWARDS_AWARD_BITS		5
#define AWARDS_WEAPON_MASK		((1 << AWARDS_WEAPON_BITS) - 1)
#define AWARDS_AWARD_MASK		((1 << AWARDS_AWARD_BITS) - 1)

#define AWARDS_PACK( weapon, first, second ) \
	( ( ( (weapon) & AWARDS_WEAPON_MASK ) << ( AWARDS_AWARD_BITS * 2 ) ) | \
	  ( ( (first) & AWARDS_AWARD_MASK ) << AWARDS_AWARD_BITS ) | \
	  ( (second) & AWARDS_AWARD_MASK ) )

#define AWARDS_WEAPON( packed )	( ( (packed) >> ( AWARDS_AWARD_BITS * 2 ) ) & AWARDS_WEAPON_MASK )
#define AWARDS_FIRST( packed )	( ( (packed) >> AWARDS_AWARD_BITS ) & AWARDS_AWARD_MASK )
#define AWARDS_SECOND( packed )	( (packed) & AWARDS_AWARD_MASK )

#endif // AWARDS_SHARED_H
