#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "com_model.h"

typedef struct
{
	short type;
	short texFormat;
	int width;
	int height;
} msprite_t;

model_s* TRI_pModel = NULL;

extern cvar_t *cl_hudscale;
extern cvar_t *cl_hudbend;

void TriHud_Init()
{
}

void TriHUD_VidInit()
{
	TRI_pModel = NULL;
}

void TRI_GetSpriteParms(model_t* pSprite, int* frameWidth, int* frameHeight)
{
	if (!pSprite || pSprite->type != mod_sprite)
		return;

	msprite_t* pSpr = (msprite_t*)pSprite->cache.data;

	*frameWidth = pSpr->width;
	*frameHeight = pSpr->height;
}

void TRI_SprAdjustSize(int* x, int* y, int* w, int* h, bool changepos, bool swap)
{
	float xscale, yscale;

	// No adjust when scaling off
	if (cl_hudscale && !cl_hudscale->value)
		return;

	if (!x && !y && !w && !h)
		return;

	float yfactor = 0;
	if ((float)ScreenHeight != 0)
		yfactor = (float)ScreenWidth / (float)ScreenHeight;

	xscale = ((float)ScreenWidth / 1080.0f) * cl_hudscale->value;
	yscale = (((float)ScreenHeight / 1080.0f) * cl_hudscale->value) * yfactor;

	if (!changepos)
	{
		xscale *= 1.5f;
		yscale *= 1.5f;
	}

	if (x)
	{
		bool swap_x = 0;
		if (*x > ScreenWidth / 2)
			swap_x = 1;
		if (swap_x)
			*x = (float)ScreenWidth - *x;
		*x *= xscale;
		if (swap_x)
			*x = (float)ScreenWidth - *x;
	}

	if (y)
	{
		bool swap_y = 0;
		if (swap && *y > ScreenHeight / 2)
			swap_y = 1;
		if (swap_y)
			*y = (float)ScreenHeight - *y;
		*y *= yscale;
		if (swap_y)
			*y = (float)ScreenHeight - *y;
	}

	if (w)
		*w *= xscale;
	if (h)
		*h *= yscale;
}

void TRI_SprDrawStretchPic(model_t* pModel, int frame, float x, float y, float w, float h, float s1, float t1, float s2, float t2)
{
	if (!pModel) // missing sprites
		return;

	gEngfuncs.pTriAPI->SpriteTexture(pModel, frame);

	float drift = (((w + h)/ 2) * cl_hudbend->value);
	if (x > (ScreenWidth / 2))
		drift *= -1;

	// bend down on top sprites
	if (y < (ScreenHeight * .75))
		drift *= -1;

/*
	float multipler = 1;
	if (x < (ScreenWidth/2))
		multipler = ((ScreenWidth/2) - x) * 0.05;
	else
		multipler = (x - (ScreenWidth/2)) * 0.05;
	multipler = min(max(multipler, 1), 4);
	drift *= multipler;*/
	//h *= (multipler / 2);
	//w *= (multipler / 2);

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	gEngfuncs.pTriAPI->TexCoord2f(s1, t1);
	gEngfuncs.pTriAPI->Vertex3f(x, y + drift, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s2, t1);
	gEngfuncs.pTriAPI->Vertex3f(x + w, y, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s2, t2);
	gEngfuncs.pTriAPI->Vertex3f(x + w, y + h, 0);

	gEngfuncs.pTriAPI->TexCoord2f(s1, t2);
	gEngfuncs.pTriAPI->Vertex3f(x, y + h + drift, 0);

	gEngfuncs.pTriAPI->End();
}

void TRI_SprDrawGeneric(int frame, int x, int y, const wrect_t* prc, bool changepos, bool swap)
{
	if (cl_hudscale && !cl_hudscale->value)
	{
		gEngfuncs.pfnSPR_Draw(frame, x, y, prc);
		return;
	}

	float s1, s2, t1, t2;
	int w, h;

	TRI_GetSpriteParms(TRI_pModel, &w, &h);

	if (w == 0 || h == 0) {
		return;
	}

	if (prc)
	{
		wrect_t rc = *prc;

		if (rc.left <= 0 || rc.left >= w)
			rc.left = 0;
		if (rc.top <= 0 || rc.top >= h)
			rc.top = 0;
		if (rc.right <= 0 || rc.right > w)
			rc.right = w;
		if (rc.bottom <= 0 || rc.bottom > h)
			rc.bottom = h;

		s1 = (float)rc.left / w;
		t1 = (float)rc.top / h;
		s2 = (float)rc.right / w;
		t2 = (float)rc.bottom / h;
		w = rc.right - rc.left;
		h = rc.bottom - rc.top;
	}
	else
	{
		s1 = t1 = 0.0f;
		s2 = t2 = 1.0f;
	}

	if (!changepos)
	{
		TRI_SprAdjustSize(&x, &y, &w, &h, changepos, swap);
		x = (ScreenWidth - w) / 2;
		y = (ScreenHeight - h) / 2;
	}
	else
		TRI_SprAdjustSize(&x, &y, &w, &h, changepos, swap);

	TRI_SprDrawStretchPic(TRI_pModel, frame, x, y, w, h, s1, t1, s2, t2);
}

void TRI_SprDrawAdditive(int frame, int x, int y, const wrect_t* prc, bool changepos, bool swap)
{
	if (cl_hudscale && !cl_hudscale->value)
	{
		gEngfuncs.pfnSPR_DrawAdditive(frame, x, y, prc);
		return;
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);

	TRI_SprDrawGeneric(frame, x, y, prc, changepos, swap);

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

void TRI_SprSet(HSPRITE spr, int r, int g, int b)
{
	if (cl_hudscale && !cl_hudscale->value)
	{
		gEngfuncs.pfnSPR_Set(spr, r, g, b);
		return;
	}

	TRI_pModel = (model_s*)gEngfuncs.GetSpritePointer(spr);
	gEngfuncs.pTriAPI->Color4ub(r, g, b, 255);
}

void TRI_FillRGBA(int x, int y, int width, int height, int r, int g, int b, int a, bool swap)
{
	if (cl_hudscale && cl_hudscale->value)
	{
		TRI_SprAdjustSize(&x, &y, &width, &height, true, swap);
	}

	gEngfuncs.pfnFillRGBA(x, y, width, height, r, g, b, a);
}

/*
void SetCrosshair(HSPRITE hspr, wrect_t rc, int r, int g, int b)
{
	gHUD.crossspr.spr = hspr;
	gHUD.crossspr.rc = rc;
	gHUD.crossspr.r = r;
	gHUD.crossspr.g = g;
	gHUD.crossspr.b = b;
}
*/
