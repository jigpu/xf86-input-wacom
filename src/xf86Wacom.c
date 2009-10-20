/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org> 
 * Copyright 2002-2009 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * This driver is currently able to handle Wacom IV, V, ISDV4, and bluetooth protocols.
 *
 * Wacom V protocol work done by Raph Levien <raph@gtk.org> and
 * Frédéric Lepied <lepied@xfree86.org>.
 *
 * Modified for Linux USB by MATSUMURA Namihiko,
 * Daniel Egger, Germany. <egger@suse.de>,
 * Frederic Lepied <lepied@xfree86.org>,
 * Brion Vibber <brion@pobox.com>,
 * Aaron Optimizer Digulla <digulla@hepe.com>,
 * Jonathan Layes <jonathan@layes.com>,
 * John Joganic <jej@j-arkadia.com>.
 * Magnus Vigerlöf <Magnus.Vigerlof@ipbo.se>.
 *
 * Many thanks to Peter Hutterer <peter.hutterer@redhat.com> 
 *		for providing Xorg, HAL and freedesktop support
 */

/*
 * REVISION HISTORY
 *
 * 2009-06-28 0.8.3-6 - Initial support for xf86-input-wacom with xorg-x11-server 1.6 and HAL
 */

static const char identification[] = "$Identification: xf86-input-wacom-0.8.3-6 $";

/****************************************************************************/

#include "xf86Wacom.h"
#include "wcmFilter.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
#include <xserver-properties.h>
#endif



void xf86WcmVirtaulTabletPadding(LocalDevicePtr local);
void xf86WcmVirtaulTabletSize(LocalDevicePtr local);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3
    extern void InitWcmDeviceProperties(LocalDevicePtr local);
    extern int xf86WcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
                BOOL checkonly);
#endif

extern Bool usbWcmInit(LocalDevicePtr pDev);
extern int usbWcmGetRanges(LocalDevicePtr local);
extern int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);
extern void xf86WcmRotateTablet(LocalDevicePtr local, int value);
extern void xf86WcmInitialScreens(LocalDevicePtr local);
extern void xf86WcmInitialCoordinates(LocalDevicePtr local, int axes);

static int xf86WcmDevOpen(DeviceIntPtr pWcm);
static void xf86WcmDevReadInput(LocalDevicePtr local);
static void xf86WcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl);
int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl * control);
static void xf86WcmDevClose(LocalDevicePtr local);
static int xf86WcmDevProc(DeviceIntPtr pWcm, int what);
static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y);
static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators);

WacomModule gWacomModule =
{
	identification, /* version */
	NULL,           /* input driver pointer */

	/* device procedures */
	xf86WcmDevOpen,
	xf86WcmDevReadInput,
	xf86WcmDevControlProc,
	xf86WcmDevChangeControl,
	xf86WcmDevClose,
	xf86WcmDevProc,
	xf86WcmDevSwitchMode,
	xf86WcmDevConvert,
	xf86WcmDevReverseConvert,
};

static void xf86WcmKbdLedCallback(DeviceIntPtr di, LedCtrl * lcp)
{
}

static void xf86WcmBellCallback(int pct, DeviceIntPtr di, pointer ctrl, int x)
{
}

static void xf86WcmKbdCtrlCallback(DeviceIntPtr di, KeybdCtrl* ctrl)
{
}

/*****************************************************************************
 * xf86WcmDesktopSize --
 *   calculate the whole desktop size 
 ****************************************************************************/
static void xf86WcmDesktopSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int i = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;

	xf86WcmInitialScreens(local);
	minX = priv->screenTopX[0];
	minY = priv->screenTopY[0];
	maxX = priv->screenBottomX[0];
	maxY = priv->screenBottomY[0];
	if (priv->numScreen != 1)
	{
		for (i = 1; i < priv->numScreen; i++)
		{
			if (priv->screenTopX[i] < minX)
				minX = priv->screenTopX[i];
			if (priv->screenTopY[i] < minY)
				minY = priv->screenTopY[i];
			if (priv->screenBottomX[i] > maxX)
				maxX = priv->screenBottomX[i];
			if (priv->screenBottomY[i] > maxY)
				maxY = priv->screenBottomY[i];
		}
	}
	priv->maxWidth = maxX - minX;
	priv->maxHeight = maxY - minY;
} 

static int xf86WcmInitArea(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomToolAreaPtr area = priv->toolarea, inlist;
	WacomCommonPtr common = priv->common;
	double screenRatio, tabletRatio;

	DBG(10, priv->debugLevel, ErrorF("xf86WcmInitArea\n"));

	/* Verify Box */
	if (priv->topX > priv->wcmMaxX)
	{
		area->topX = priv->topX = 0;
	}

	if (priv->topY > priv->wcmMaxY)
	{
		area->topY = priv->topY = 0;
	}

	/* set unconfigured bottom to max */
	priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
	if (priv->bottomX < priv->topX || !priv->bottomX)
	{
		area->bottomX = priv->bottomX = priv->wcmMaxX;
	}

	/* set unconfigured bottom to max */
	priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
	if (priv->bottomY < priv->topY || !priv->bottomY)
	{
		area->bottomY = priv->bottomY = priv->wcmMaxY;
	}

	if (priv->twinview != TV_NONE)
		priv->numScreen = 2;

	if (priv->screen_no != -1 &&
		(priv->screen_no >= priv->numScreen || priv->screen_no < 0))
	{
		if (priv->twinview == TV_NONE || priv->screen_no != 1)
		{
			ErrorF("%s: invalid screen number %d, resetting to default (-1) \n",
					local->name, priv->screen_no);
			priv->screen_no = -1;
		}
	}

	/* need maxWidth and maxHeight for keepshape */
	xf86WcmDesktopSize(local);

	/* Maintain aspect ratio to the whole desktop
	 * May need to consider a specific screen in multimonitor settings
	 */
	if (priv->flags & KEEP_SHAPE_FLAG)
	{

		screenRatio = ((double)priv->maxWidth / (double)priv->maxHeight);
		tabletRatio = ((double)(priv->wcmMaxX - priv->topX) /
				(double)(priv->wcmMaxY - priv->topY));

		DBG(2, priv->debugLevel, ErrorF("screenRatio = %.3g, "
			"tabletRatio = %.3g\n", screenRatio, tabletRatio));

		if (screenRatio > tabletRatio)
		{
			area->bottomX = priv->bottomX = priv->wcmMaxX;
			area->bottomY = priv->bottomY = (priv->wcmMaxY - priv->topY) *
				tabletRatio / screenRatio + priv->topY;
		}
		else
		{
			area->bottomX = priv->bottomX = (priv->wcmMaxX - priv->topX) *
				screenRatio / tabletRatio + priv->topX;
			area->bottomY = priv->bottomY = priv->wcmMaxY;
		}
	}
	/* end keep shape */ 

	inlist = priv->tool->arealist;

	/* The first one in the list is always valid */
	if (area != inlist && xf86WcmAreaListOverlap(area, inlist))
	{
		inlist = priv->tool->arealist;

		/* remove this overlapped area from the list */
		for (; inlist; inlist=inlist->next)
		{
			if (inlist->next == area)
			{
				inlist->next = area->next;
				xfree(area);
				priv->toolarea = NULL;
 			break;
			}
		}

		/* Remove this device from the common struct */
		if (common->wcmDevices == priv)
			common->wcmDevices = priv->next;
		else
		{
			WacomDevicePtr tmp = common->wcmDevices;
			while(tmp->next && tmp->next != priv)
				tmp = tmp->next;
			if(tmp)
				tmp->next = priv->next;
		}
		xf86Msg(X_ERROR, "%s: Top/Bottom area overlaps with another devices.\n",
			local->conf_idev->identifier);
		return FALSE;
	}
	if (xf86Verbose)
	{
		ErrorF("%s Wacom device \"%s\" top X=%d top Y=%d "
				"bottom X=%d bottom Y=%d "
				"resol X=%d resol Y=%d\n",
				XCONFIG_PROBED, local->name, priv->topX,
				priv->topY, priv->bottomX, priv->bottomY,
				priv->wcmResolX, priv->wcmResolY);
	}
	return TRUE;
}

/*****************************************************************************
 * xf86WcmVirtaulTabletPadding(LocalDevicePtr local)
 ****************************************************************************/

void xf86WcmVirtaulTabletPadding(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int i;

	priv->leftPadding = 0;
	priv->topPadding = 0;

	if (!(priv->flags & ABSOLUTE_FLAG)) return;

	if ((priv->screen_no != -1) || (priv->twinview != TV_NONE) || (!priv->wcmMMonitor))
	{
		i = priv->currentScreen;

		priv->leftPadding = priv->bottomX - priv->topX -priv->tvoffsetX;
 		priv->topPadding = priv->bottomY - priv->topY - priv->tvoffsetY;

		priv->leftPadding = (int)(((double)priv->screenTopX[i] * priv->leftPadding )
			/ ((double)(priv->screenBottomX[i] - priv->screenTopX[i])) + 0.5);

		priv->topPadding = (int)((double)(priv->screenTopY[i] * priv->topPadding)
			/ ((double)(priv->screenBottomY[i] - priv->screenTopY[i])) + 0.5);
	}
	DBG(10, priv->debugLevel, ErrorF("xf86WcmVirtaulTabletPadding for \"%s\" "
		"x=%d y=%d \n", local->name, priv->leftPadding, priv->topPadding));
	return;
}

/*****************************************************************************
 * xf86WcmVirtaulTabletSize(LocalDevicePtr local)
 ****************************************************************************/

void xf86WcmVirtaulTabletSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int i, tabletSize;

	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		priv->sizeX = priv->bottomX - priv->topX;
		priv->sizeY = priv->bottomY - priv->topY;
		return;
	}

	priv->sizeX = priv->bottomX - priv->topX - priv->tvoffsetX;
	priv->sizeY = priv->bottomY - priv->topY - priv->tvoffsetY;

	if ((priv->screen_no != -1) || (priv->twinview != TV_NONE) || (!priv->wcmMMonitor))
	{
		i = priv->currentScreen;

		tabletSize = priv->sizeX;
		priv->sizeX += (int)(((double)priv->screenTopX[i] * tabletSize)
			/ ((double)(priv->screenBottomX[i] - priv->screenTopX[i])) + 0.5);
		priv->sizeX += (int)((double)((priv->maxWidth - priv->screenBottomX[i])
			* tabletSize) / ((double)(priv->screenBottomX[i] - priv->screenTopX[i])) + 0.5);

		tabletSize = priv->sizeY;
		priv->sizeY += (int)((double)(priv->screenTopY[i] * tabletSize)
			/ ((double)(priv->screenBottomY[i] - priv->screenTopY[i])) + 0.5);
		priv->sizeY += (int)((double)((priv->maxHeight - priv->screenBottomY[i])
			* tabletSize) / ((double)(priv->screenBottomY[i] - priv->screenTopY[i])) + 0.5);
	}
	DBG(10, priv->debugLevel, ErrorF("xf86WcmVirtaulTabletSize for \"%s\" "
		"x=%d y=%d \n", local->name, priv->sizeX, priv->sizeY));
	return;
}

/*****************************************************************************
 * xf86WcmInitialCoordinates
 ****************************************************************************/

void xf86WcmInitialCoordinates(LocalDevicePtr local, int axes)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int topx = 0, topy = 0, resolution;
	int bottomx = priv->wcmMaxX, bottomy = priv->wcmMaxY;
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
        Atom label;
#endif

	xf86WcmMappingFactor(local);

	/* x ax */
	if ( !axes )
	{
		if (priv->flags & ABSOLUTE_FLAG)
		{
			topx = priv->topX;
			bottomx = priv->sizeX + priv->topX;
			if (priv->currentScreen == 1 && priv->twinview != TV_NONE)
				topx += priv->tvoffsetX;
			if (priv->currentScreen == 0 && priv->twinview != TV_NONE)
				bottomx -= priv->tvoffsetX;

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                        label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X);
                } else {
                        label = XIGetKnownProperty(AXIS_LABEL_PROP_REL_X);
#endif
		}

		resolution = priv->wcmResolX;
		if (common->wcmScaling)
		{
			/* In case xf86WcmDevConvert didn't get called */
			topx = 0;
			bottomx = (int)((double)priv->sizeX * priv->factorX + 0.5);
			resolution = (int)((double)resolution * priv->factorX + 0.5);
		}

		InitValuatorAxisStruct(local->dev, 0,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                        label,
#endif
                        topx, bottomx,
			resolution, 0, resolution);
	}
	else /* y ax */
	{
		if (priv->flags & ABSOLUTE_FLAG)
		{
			topy = priv->topY;
			bottomy = priv->sizeY + priv->topY;
			if (priv->currentScreen == 1 && priv->twinview != TV_NONE)
				topy += priv->tvoffsetY;
			if (priv->currentScreen == 0 && priv->twinview != TV_NONE)
				bottomy -= priv->tvoffsetY;

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                        label = XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y);
                } else {
                        label = XIGetKnownProperty(AXIS_LABEL_PROP_REL_Y);
#endif
		}

		resolution = priv->wcmResolY;
		if (common->wcmScaling)
		{
			/* In case xf86WcmDevConvert didn't get called */
			topy = 0;
			bottomy = (int)((double)priv->sizeY * priv->factorY + 0.5);
			resolution = (int)((double)resolution * priv->factorY + 0.5);
		}

		InitValuatorAxisStruct(local->dev, 1,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
                        label,
#endif
                        topy, bottomy,
			resolution, 0, resolution);
	}
	return;
}

/*****************************************************************************
 * xf86WcmRegisterX11Devices --
 *    Register the X11 input devices with X11 core.
 ****************************************************************************/


#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 5
/* Define our own keymap so we can send key-events with our own device and not
 * rely on inputInfo.keyboard */
static KeySym keymap[] = {
	/* 0x00 */  NoSymbol,		NoSymbol,	XK_Escape,	NoSymbol,
	/* 0x02 */  XK_1,		XK_exclam,	XK_2,		XK_at,
	/* 0x04 */  XK_3,		XK_numbersign,	XK_4,		XK_dollar,
	/* 0x06 */  XK_5,		XK_percent,	XK_6,		XK_asciicircum,
	/* 0x08 */  XK_7,		XK_ampersand,	XK_8,		XK_asterisk,
	/* 0x0a */  XK_9,		XK_parenleft,	XK_0,		XK_parenright,
	/* 0x0c */  XK_minus,		XK_underscore,	XK_equal,	XK_plus,
	/* 0x0e */  XK_BackSpace,	NoSymbol,	XK_Tab,		XK_ISO_Left_Tab,
	/* 0x10 */  XK_q,		NoSymbol,	XK_w,		NoSymbol,
	/* 0x12 */  XK_e,		NoSymbol,	XK_r,		NoSymbol,
	/* 0x14 */  XK_t,		NoSymbol,	XK_y,		NoSymbol,
	/* 0x16 */  XK_u,		NoSymbol,	XK_i,		NoSymbol,
	/* 0x18 */  XK_o,		NoSymbol,	XK_p,		NoSymbol,
	/* 0x1a */  XK_bracketleft,	XK_braceleft,	XK_bracketright,	XK_braceright,
	/* 0x1c */  XK_Return,		NoSymbol,	XK_Control_L,	NoSymbol,
	/* 0x1e */  XK_a,		NoSymbol,	XK_s,		NoSymbol,
	/* 0x20 */  XK_d,		NoSymbol,	XK_f,		NoSymbol,
	/* 0x22 */  XK_g,		NoSymbol,	XK_h,		NoSymbol,
	/* 0x24 */  XK_j,		NoSymbol,	XK_k,		NoSymbol,
	/* 0x26 */  XK_l,		NoSymbol,	XK_semicolon,	XK_colon,
	/* 0x28 */  XK_quoteright,	XK_quotedbl,	XK_quoteleft,	XK_asciitilde,
	/* 0x2a */  XK_Shift_L,		NoSymbol,	XK_backslash,	XK_bar,
	/* 0x2c */  XK_z,		NoSymbol,	XK_x,		NoSymbol,
	/* 0x2e */  XK_c,		NoSymbol,	XK_v,		NoSymbol,
	/* 0x30 */  XK_b,		NoSymbol,	XK_n,		NoSymbol,
	/* 0x32 */  XK_m,		NoSymbol,	XK_comma,	XK_less,
	/* 0x34 */  XK_period,		XK_greater,	XK_slash,	XK_question,
	/* 0x36 */  XK_Shift_R,		NoSymbol,	XK_KP_Multiply,	NoSymbol,
	/* 0x38 */  XK_Alt_L,		XK_Meta_L,	XK_space,	NoSymbol,
	/* 0x3a */  XK_Caps_Lock,	NoSymbol,	XK_F1,		NoSymbol,
	/* 0x3c */  XK_F2,		NoSymbol,	XK_F3,		NoSymbol,
	/* 0x3e */  XK_F4,		NoSymbol,	XK_F5,		NoSymbol,
	/* 0x40 */  XK_F6,		NoSymbol,	XK_F7,		NoSymbol,
	/* 0x42 */  XK_F8,		NoSymbol,	XK_F9,		NoSymbol,
	/* 0x44 */  XK_F10,		NoSymbol,	XK_Num_Lock,	NoSymbol,
	/* 0x46 */  XK_Scroll_Lock,	NoSymbol,	XK_KP_Home,	XK_KP_7,
	/* 0x48 */  XK_KP_Up,		XK_KP_8,	XK_KP_Prior,	XK_KP_9,
	/* 0x4a */  XK_KP_Subtract,	NoSymbol,	XK_KP_Left,	XK_KP_4,
	/* 0x4c */  XK_KP_Begin,	XK_KP_5,	XK_KP_Right,	XK_KP_6,
	/* 0x4e */  XK_KP_Add,		NoSymbol,	XK_KP_End,	XK_KP_1,
	/* 0x50 */  XK_KP_Down,		XK_KP_2,	XK_KP_Next,	XK_KP_3,
	/* 0x52 */  XK_KP_Insert,	XK_KP_0,	XK_KP_Delete,	XK_KP_Decimal,
	/* 0x54 */  NoSymbol,		NoSymbol,	XK_F13,		NoSymbol,
	/* 0x56 */  XK_less,		XK_greater,	XK_F11,		NoSymbol,
	/* 0x58 */  XK_F12,		NoSymbol,	XK_F14,		NoSymbol,
	/* 0x5a */  XK_F15,		NoSymbol,	XK_F16,		NoSymbol,
	/* 0x5c */  XK_F17,		NoSymbol,	XK_F18,		NoSymbol,
	/* 0x5e */  XK_F19,		NoSymbol,	XK_F20,		NoSymbol,
	/* 0x60 */  XK_KP_Enter,	NoSymbol,	XK_Control_R,	NoSymbol,
	/* 0x62 */  XK_KP_Divide,	NoSymbol,	XK_Print,	XK_Sys_Req,
	/* 0x64 */  XK_Alt_R,		XK_Meta_R,	NoSymbol,	NoSymbol,
	/* 0x66 */  XK_Home,		NoSymbol,	XK_Up,		NoSymbol,
	/* 0x68 */  XK_Prior,		NoSymbol,	XK_Left,	NoSymbol,
	/* 0x6a */  XK_Right,		NoSymbol,	XK_End,		NoSymbol,
	/* 0x6c */  XK_Down,		NoSymbol,	XK_Next,	NoSymbol,
	/* 0x6e */  XK_Insert,		NoSymbol,	XK_Delete,	NoSymbol,
	/* 0x70 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x72 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x74 */  NoSymbol,		NoSymbol,	XK_KP_Equal,	NoSymbol,
	/* 0x76 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x78 */  XK_F21,		NoSymbol,	XK_F22,		NoSymbol,
	/* 0x7a */  XK_F23,		NoSymbol,	XK_F24,		NoSymbol,
	/* 0x7c */  XK_KP_Separator,	NoSymbol,	XK_Meta_L,	NoSymbol,
	/* 0x7e */  XK_Meta_R,		NoSymbol,	XK_Multi_key,	NoSymbol,
	/* 0x80 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x82 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x84 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x86 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x88 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x8a */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x8c */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x8e */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x90 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x92 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x94 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x96 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x98 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x9a */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x9c */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x9e */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xaa */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xac */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xae */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xba */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xbc */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xbe */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xca */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xcc */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xce */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xda */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xdc */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xde */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xea */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xec */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xee */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol
};

static struct { KeySym keysym; CARD8 mask; } keymod[] = {
	{ XK_Shift_L,	ShiftMask },
	{ XK_Shift_R,	ShiftMask },
	{ XK_Control_L,	ControlMask },
	{ XK_Control_R,	ControlMask },
	{ XK_Caps_Lock,	LockMask },
	{ XK_Alt_L,	Mod1Mask }, /*AltMask*/
	{ XK_Alt_R,	Mod1Mask }, /*AltMask*/
	{ XK_Num_Lock,	Mod2Mask }, /*NumLockMask*/
	{ XK_Scroll_Lock,	Mod5Mask }, /*ScrollLockMask*/
	{ XK_Mode_switch,	Mod3Mask }, /*AltMask*/
	{ NoSymbol,	0 }
};
#endif

/*****************************************************************************
 * xf86WcmInitialprivSize --
 *    Initialize logical size and resolution for individual tool.
 ****************************************************************************/

static void xf86WcmInitialToolSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomToolPtr toollist = common->wcmTool;
	WacomToolAreaPtr arealist;

	if (IsTouch(priv))
	{
		priv->wcmMaxX = common->wcmMaxTouchX;
		priv->wcmMaxY = common->wcmMaxTouchY;
		priv->wcmResolX = common->wcmTouchResolX;
		priv->wcmResolY = common->wcmTouchResolY;
	}
	else
	{
		priv->wcmMaxX = common->wcmMaxX;
		priv->wcmMaxY = common->wcmMaxY;
		priv->wcmResolX = common->wcmResolX;
		priv->wcmResolY = common->wcmResolY;
	}

	for (; toollist; toollist=toollist->next)
	{
		arealist = toollist->arealist;
		for (; arealist; arealist=arealist->next)
		{
			if (!arealist->bottomX) 
				arealist->bottomX = priv->wcmMaxX;
			if (!arealist->bottomY)
				arealist->bottomY = priv->wcmMaxY;
		}
	}

	return;
}

/*****************************************************************************
 * xf86WcmRegisterX11Devices --
 *    Register the X11 input devices with X11 core.
 ****************************************************************************/

static int xf86WcmRegisterX11Devices (LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	unsigned char butmap[WCM_MAX_BUTTONS+1];
	int nbaxes, nbbuttons, nbkeys;
	int loop;
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
        Atom btn_labels[WCM_MAX_BUTTONS] = {0};
        Atom axis_labels[MAX_VALUATORS] = {0};
#endif

	/* Detect tablet configuration, if possible */
	if (priv->common->wcmModel->DetectConfig)
		priv->common->wcmModel->DetectConfig (local);

	nbaxes = priv->naxes;       /* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	nbbuttons = priv->nbuttons; /* Use actual number of buttons, if possible */
	nbkeys = nbbuttons;         /* Same number of keys since any button may be 
	                             * configured as an either mouse button or key */

	if (!nbbuttons)
		nbbuttons = nbkeys = 1;	    /* Xserver 1.5 or later crashes when 
			            	     * nbbuttons = 0 while sending a beep 
			             	     * This is only a workaround. 
				     	     */

	DBG(10, priv->debugLevel, ErrorF("xf86WcmRegisterX11Devices "
		"(%s) %d buttons, %d keys, %d axes\n",
		IsStylus(priv) ? "stylus" :
		IsCursor(priv) ? "cursor" :
		IsPad(priv) ? "pad" : "eraser",
		nbbuttons, nbkeys, nbaxes));

	for(loop=1; loop<=nbbuttons; loop++)
		butmap[loop] = loop;

	/* FIXME: button labels would be nice */
	if (InitButtonClassDeviceStruct(local->dev, nbbuttons,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					btn_labels,
#endif
					butmap) == FALSE)
	{
		ErrorF("unable to allocate Button class device\n");
		return FALSE;
	}

	if (InitFocusClassDeviceStruct(local->dev) == FALSE)
	{
		ErrorF("unable to init Focus class device\n");
		return FALSE;
	}

	if (InitPtrFeedbackClassDeviceStruct(local->dev,
		xf86WcmDevControlProc) == FALSE)
	{
		ErrorF("unable to init ptr feedback\n");
		return FALSE;
	}

	if (InitProximityClassDeviceStruct(local->dev) == FALSE)
	{
			ErrorF("unable to init proximity class device\n");
			return FALSE;
	}

	if (!nbaxes || nbaxes > 6)
		nbaxes = priv->naxes = 6;

	/* axis_labels is just zeros, we set up each valuator with the
	 * correct property later */
	if (InitValuatorClassDeviceStruct(local->dev, nbaxes,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					  axis_labels,
#endif
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
					  GetMotionHistory,
#endif
					  GetMotionHistorySize(),
					  ((priv->flags & ABSOLUTE_FLAG) ?
					  Absolute : Relative) | 
					  OutOfProximity ) == FALSE)
	{
		ErrorF("unable to allocate Valuator class device\n");
		return FALSE;
	}


	/* only initial KeyClass and LedFeedbackClass once */
	if (!priv->wcmInitKeyClassCount)
	{
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 5
		if (nbkeys)
		{
			KeySymsRec wacom_keysyms;
			CARD8 modmap[MAP_LENGTH];
			int i,j;

			memset(modmap, 0, sizeof(modmap));
			for(i=0; keymod[i].keysym != NoSymbol; i++)
				for(j=8; j<256; j++)
					if(keymap[(j-8)*2] == keymod[i].keysym)
						modmap[j] = keymod[i].mask;

			/* There seems to be a long-standing misunderstanding about
			 * how a keymap should be defined. All tablet drivers from
			 * stock X11 source tree are doing it wrong: they leave first
			 * 8 keysyms as VoidSymbol's, and are passing 8 as minimum
			 * key code. But if you look at SetKeySymsMap() from
			 * programs/Xserver/dix/devices.c you will see that
			 * Xserver does not require first 8 keysyms; it supposes
			 * that the map begins at minKeyCode.
			 *
			 * It could be that this assumption is a leftover from
			 * earlier XFree86 versions, but that's out of our scope.
			 * This also means that no keys on extended input devices
			 * with their own keycodes (e.g. tablets) were EVER used.
			 */
			wacom_keysyms.map = keymap;
			/* minKeyCode = 8 because this is the min legal key code */
			wacom_keysyms.minKeyCode = 8;
			wacom_keysyms.maxKeyCode = 255;
			wacom_keysyms.mapWidth = 2;
			if (InitKeyClassDeviceStruct(local->dev, &wacom_keysyms, modmap) == FALSE)
			{
				ErrorF("unable to init key class device\n");
				return FALSE;
			}
		}

		if(InitKbdFeedbackClassDeviceStruct(local->dev, xf86WcmBellCallback,
				xf86WcmKbdCtrlCallback) == FALSE) {
			ErrorF("unable to init kbd feedback device struct\n");
			return FALSE;
		}
#endif
		if(InitLedFeedbackClassDeviceStruct (local->dev, xf86WcmKbdLedCallback) == FALSE) {
			ErrorF("unable to init led feedback device struct\n");
			return FALSE;
		}
	}

 	xf86WcmInitialToolSize(local);

	if (xf86WcmInitArea(local) == FALSE)
	{
		return FALSE;
	}

	/* Rotation rotates the Max X and Y */
	xf86WcmRotateTablet(local, common->wcmRotate);

	/* pressure */
	InitValuatorAxisStruct(local->dev, 2,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_PRESSURE),
#endif
		0, common->wcmMaxZ, 1, 1, 1);

	if (IsCursor(priv))
	{
		/* z-rot and throttle */
		InitValuatorAxisStruct(local->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_RZ),
#endif
		-900, 899, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_THROTTLE),
#endif
		-1023, 1023, 1, 1, 1);
	}
	else if (IsPad(priv))
	{
		/* strip-x and strip-y */
		if (strstr(common->wcmModel->name, "Intuos3") || 
			strstr(common->wcmModel->name, "CintiqV5")) 
		{
			InitValuatorAxisStruct(local->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, common->wcmMaxStripX, 1, 1, 1);
			InitValuatorAxisStruct(local->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, common->wcmMaxStripY, 1, 1, 1);
		}
	}
	else
	{
		/* tilt-x and tilt-y */
		InitValuatorAxisStruct(local->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_X),
#endif
				-64, 63, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_Y),
#endif
				-64, 63, 1, 1, 1);
	}

	if ((strstr(common->wcmModel->name, "Intuos3") || 
		strstr(common->wcmModel->name, "CintiqV5") ||
		strstr(common->wcmModel->name, "Intuos4")) 
			&& IsStylus(priv))
		/* Art Marker Pen rotation */
		InitValuatorAxisStruct(local->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				-900, 899, 1, 1, 1);
	else if ((strstr(common->wcmModel->name, "Bamboo") ||
		strstr(common->wcmModel->name, "Intuos4"))
			&& IsPad(priv))
		/* Touch ring */
		InitValuatorAxisStruct(local->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, 71, 1, 1, 1);
	else
	{
		/* absolute wheel */
		InitValuatorAxisStruct(local->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL),
#endif
				0, 1023, 1, 1, 1);
	}

	if (IsTouch(priv))
	{
		/* hard prox out */
		priv->hardProx = 0;
	}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3
	InitWcmDeviceProperties(local);
	XIRegisterPropertyHandler(local->dev, xf86WcmSetProperty, NULL, NULL);
#endif

	return TRUE;
}

static Bool xf86WcmIsWacomDevice (int fd, CARD16 vendor)
{
	struct input_id id;
	ioctl(fd, EVIOCGID, &id);
	if (id.vendor == vendor)
		return TRUE;
	return FALSE;
}

/*****************************************************************************
 * xf86WcmEventAutoDevProbe -- Probe for right input device
 ****************************************************************************/
#define DEV_INPUT_EVENT "/dev/input/event%d"
#define EVDEV_MINORS    32
char *xf86WcmEventAutoDevProbe (LocalDevicePtr local)
{
	/* We are trying to find the right eventX device */
	int i, wait = 0;
	const int max_wait = 2000;

	/* If device is not available after Resume, wait some ms */
	while (wait <= max_wait) 
	{
		for (i = 0; i < EVDEV_MINORS; i++) 
		{
			char fname[64];
			int fd = -1;
			Bool is_wacom;

			sprintf(fname, DEV_INPUT_EVENT, i);
			SYSCALL(fd = open(fname, O_RDONLY));
			if (fd < 0)
				continue;
			is_wacom = xf86WcmIsWacomDevice(fd, 0x056a);
			SYSCALL(close(fd));
			if (is_wacom) 
			{
				ErrorF ("%s Wacom probed device to be %s (waited %d msec)\n",
					XCONFIG_PROBED, fname, wait);
				xf86ReplaceStrOption(local->options, "Device", fname);
				return xf86FindOptionValue(local->options, "Device");
			}
		}
		wait += 100;
		ErrorF("%s waiting 100 msec (total %dms) for device to become ready\n", local->name, wait); 
		usleep(100*1000);
	}
	ErrorF("%s no Wacom event device found (checked %d nodes, waited %d msec)\n",
		local->name, i + 1, wait);
	return FALSE;
}

/*****************************************************************************
 * xf86WcmDevOpen --
 *    Open the physical device and init information structs.
 ****************************************************************************/

static int xf86WcmDevOpen(DeviceIntPtr pWcm)
{
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)PRIVATE(pWcm);
	WacomCommonPtr common = priv->common;
 
	DBG(10, priv->debugLevel, ErrorF("xf86WcmDevOpen\n"));

	/* Device has been open and not autoprobed */
	if (priv->wcmDevOpenCount)
		return TRUE;

	/* open file, if not already open */
	if (common->fd_refs == 0)
	{
		/* Autoprobe if necessary */
		if ((common->wcmFlags & AUTODEV_FLAG) &&
		    !(common->wcmDevice = xf86WcmEventAutoDevProbe (local)))
			ErrorF("Cannot probe device\n");

		if ((xf86WcmOpen (local) != Success) || (local->fd < 0) ||
			!common->wcmDevice)
		{
			DBG(1, priv->debugLevel, ErrorF("Failed to open "
				"device (fd=%d)\n", local->fd));
			if (local->fd >= 0)
			{
				DBG(1, priv->debugLevel, ErrorF("Closing device\n"));
				xf86WcmClose(local->fd);
			}
			local->fd = -1;
			return FALSE;
		}
		common->fd = local->fd;
		common->fd_refs = 1;
	}

	/* Grab the common descriptor, if it's available */
	if (local->fd < 0)
	{
		local->fd = common->fd;
		common->fd_refs++;
	}

	if (!xf86WcmRegisterX11Devices (local))
		return FALSE;

	return TRUE;
}

/*****************************************************************************
 * xf86WcmDevReadInput --
 *   Read the device on IO signal
 ****************************************************************************/

static void xf86WcmDevReadInput(LocalDevicePtr local)
{
	int loop=0;
	#define MAX_READ_LOOPS 10

	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	/* move data until we exhaust the device */
	for (loop=0; loop < MAX_READ_LOOPS; ++loop)
	{
		/* verify that there is still data in pipe */
		if (!xf86WcmReady(local)) break;

		/* dispatch */
		common->wcmDevCls->Read(local);
	}

	/* report how well we're doing */
	if (loop >= MAX_READ_LOOPS)
		DBG(1, priv->debugLevel, ErrorF("xf86WcmDevReadInput: Can't keep up!!!\n"));
	else if (loop > 0)
		DBG(10, priv->debugLevel, ErrorF("xf86WcmDevReadInput: Read (%d)\n",loop));
}					

void xf86WcmReadPacket(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int len, pos, cnt, remaining;
	unsigned char * data;

 	DBG(10, common->debugLevel, ErrorF("xf86WcmReadPacket: device=%s"
		" fd=%d \n", common->wcmDevice, local->fd));

	remaining = sizeof(common->buffer) - common->bufpos;

	DBG(1, common->debugLevel, ErrorF("xf86WcmReadPacket: pos=%d"
		" remaining=%d\n", common->bufpos, remaining));

	/* fill buffer with as much data as we can handle */
	len = xf86WcmRead(local->fd,
		common->buffer + common->bufpos, remaining);

	if (len <= 0)
	{
		/* In case of error, we assume the device has been
		 * disconnected. So we close it and iterate over all
		 * wcmDevices to actually close associated devices. */
		WacomDevicePtr wDev = common->wcmDevices;
		for(; wDev; wDev = wDev->next)
		{
			if (wDev->local->fd >= 0)
				xf86WcmDevProc(wDev->local->dev, DEVICE_OFF);
		}
		ErrorF("Error reading wacom device : %s\n", strerror(errno));
		return;
	}

	/* account for new data */
	common->bufpos += len;
	DBG(10, common->debugLevel, ErrorF("xf86WcmReadPacket buffer has %d bytes\n",
		common->bufpos));

	pos = 0;

	/* while there are whole packets present, check the packet length
	 * for serial ISDv4 packet since it's different for pen and touch
	 */
	if (common->wcmForceDevice == DEVICE_ISDV4 && common->wcmDevCls != &gWacomUSBDevice) 
	{
		common->wcmPktLength = 9;
		data = common->buffer;
		if ( data[0] & 0x18 )
		{
			if (common->wcmMaxCapacity)
				common->wcmPktLength = 7;
			else
				common->wcmPktLength = 5;
		}
	}

	while ((common->bufpos - pos) >=  common->wcmPktLength)
	{
		/* parse packet */
		cnt = common->wcmModel->Parse(local, common->buffer + pos);
		if (cnt <= 0)
		{
			DBG(1, common->debugLevel, ErrorF("Misbehaving parser returned %d\n",cnt));
			break;
		}
		pos += cnt;

		if (common->wcmDevCls != &gWacomUSBDevice) 
		{
			data = common->buffer + pos;
			if ( data[0] & 0x18 )
			{
				if (common->wcmPktLength == 9)
				{
					DBG(1, common->debugLevel, 
						ErrorF("xf86WcmReadPacket: not a pen data any more \n"));
					break;	
				}
			}
			else
			{
				if (common->wcmPktLength != 9)
				{
					DBG(1, common->debugLevel, 
						ErrorF("xf86WcmReadPacket: not a touch data any more \n"));
					break;	
				}
			}
		}
	}
 
	if (pos)
	{
		/* if half a packet remains, move it down */
		if (pos < common->bufpos)
		{
			DBG(7, common->debugLevel, ErrorF("MOVE %d bytes\n", common->bufpos - pos));
			memmove(common->buffer,common->buffer+pos,
				common->bufpos-pos);
			common->bufpos -= pos;
		}

		/* otherwise, reset the buffer for next time */
		else
		{
			common->bufpos = 0;
		}
	}
}

int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl * control)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	DBG(3, priv->debugLevel, ErrorF("xf86WcmDevChangeControl called\n"));
	return Success;
}

/*****************************************************************************
 * xf86WcmDevControlProc --
 ****************************************************************************/

static void xf86WcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl)
{
	LocalDevicePtr local = (LocalDevicePtr)device->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(4, priv->debugLevel, ErrorF("Wacom Dev Control Proc called\n"));
	return;
}

/*****************************************************************************
 * xf86WcmDevClose --
 ****************************************************************************/

static void xf86WcmDevClose(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(4, priv->debugLevel, ErrorF("Wacom number of open devices = %d\n", common->fd_refs));

	if (local->fd >= 0)
	{
		local->fd = -1;
		if (!--common->fd_refs)
		{
			DBG(1, common->debugLevel, ErrorF("Closing device; uninitializing.\n"));
	    		xf86WcmClose (common->fd);
		}
	}
}
 
/*****************************************************************************
 * xf86WcmDevProc --
 *   Handle the initialization, etc. of a wacom
 ****************************************************************************/

static int xf86WcmDevProc(DeviceIntPtr pWcm, int what)
{
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)PRIVATE(pWcm);

	DBG(2, priv->debugLevel, ErrorF("BEGIN xf86WcmProc dev=%p priv=%p "
			"type=%s(%s) flags=%d fd=%d what=%s\n",
			(void *)pWcm, (void *)priv,
			IsStylus(priv) ? "stylus" :
			IsCursor(priv) ? "cursor" :
			IsPad(priv) ? "pad" : "eraser", 
			local->name, priv->flags, local ? local->fd : -1,
			(what == DEVICE_INIT) ? "INIT" :
			(what == DEVICE_OFF) ? "OFF" :
			(what == DEVICE_ON) ? "ON" :
			(what == DEVICE_CLOSE) ? "CLOSE" : "???"));

	switch (what)
	{
		/* All devices must be opened here to initialize and
		 * register even a 'pad' which doesn't "SendCoreEvents"
		 */
		case DEVICE_INIT:
			priv->wcmDevOpenCount = 0;
			priv->wcmInitKeyClassCount = 0;
			if (!xf86WcmDevOpen(pWcm))
			{
				DBG(1, priv->debugLevel, ErrorF("xf86WcmProc INIT FAILED\n"));
				return !Success;
			}
			priv->wcmInitKeyClassCount++;
			priv->wcmDevOpenCount++;
			break; 

		case DEVICE_ON:
			if (!xf86WcmDevOpen(pWcm))
			{
				DBG(1, priv->debugLevel, ErrorF("xf86WcmProc ON FAILED\n"));
				return !Success;
			}
			priv->wcmDevOpenCount++;
			xf86AddEnabledDevice(local);
			pWcm->public.on = TRUE;
			break;

		case DEVICE_OFF:
		case DEVICE_CLOSE:
			if (local->fd >= 0)
			{
				xf86RemoveEnabledDevice(local);
				xf86WcmDevClose(local);
			}
			pWcm->public.on = FALSE;
			priv->wcmDevOpenCount = 0;
			break;

		default:
			ErrorF("wacom unsupported mode=%d\n", what);
			return !Success;
			break;
	} /* end switch */

	DBG(2, priv->debugLevel, ErrorF("END xf86WcmProc Success \n"));
	return Success;
}

/*****************************************************************************
 * xf86WcmDevConvert --
 *  Convert X & Y valuators so core events can be generated with 
 *  coordinates that are scaled and suitable for screen resolution.
 ****************************************************************************/

static Bool xf86WcmDevConvert(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
    
	DBG(6, priv->debugLevel, ErrorF("xf86WcmDevConvert v0=%d v1=%d on screen %d \n",
		 v0, v1, priv->currentScreen));

	if (first != 0 || num == 1) 
 		return FALSE;

	if (priv->flags & ABSOLUTE_FLAG)
	{
		v0 -= priv->topX;
		v1 -= priv->topY;
		if (priv->currentScreen == 1 && priv->twinview != TV_NONE)
		{
			v0 -= priv->tvoffsetX;
			v1 -= priv->tvoffsetY;
		}
 	}

	*x = (double)v0 * priv->factorX + 0.5;
	*y = (double)v1 * priv->factorY + 0.5;

	if ((priv->flags & ABSOLUTE_FLAG) && (priv->twinview == TV_NONE))
	{
		*x -= priv->screenTopX[priv->currentScreen];
		*y -= priv->screenTopY[priv->currentScreen];
	}

	if (priv->screen_no != -1)
	{
		if (*x > priv->screenBottomX[priv->currentScreen] - priv->screenTopX[priv->currentScreen])
			*x = priv->screenBottomX[priv->currentScreen];
		if (*x < 0) *x = 0;
		if (*y > priv->screenBottomY[priv->currentScreen] - priv->screenTopY[priv->currentScreen])
			*y = priv->screenBottomY[priv->currentScreen];
		if (*y < 0) *y = 0;
	
	}
	DBG(6, priv->debugLevel, ErrorF("xf86WcmDevConvert v0=%d v1=%d to x=%d y=%d\n", v0, v1, *x, *y));
	return TRUE;
}

/*****************************************************************************
 * xf86WcmDevReverseConvert --
 *  Convert X and Y to valuators in relative mode where the position of 
 *  the core pointer must be translated into device cootdinates before 
 *  the extension and core events are generated in Xserver.
 ****************************************************************************/

static Bool xf86WcmDevReverseConvert(LocalDevicePtr local, int x, int y,
		int* valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int i = 0;

	DBG(6, priv->debugLevel, ErrorF("xf86WcmDevReverseConvert x=%d y=%d \n", x, y));
	priv->currentSX = x;
	priv->currentSY = y;

	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		if (!priv->devReverseCount)
		{
			valuators[0] = (((double)x / priv->factorX) + 0.5);
			valuators[1] = (((double)y / priv->factorY) + 0.5);

			/* reset valuators to report raw values */
			for (i=2; i<priv->naxes; i++)
				valuators[i] = 0;

			priv->devReverseCount = 1;
		}
		else
			priv->devReverseCount = 0;
	}

	DBG(6, priv->debugLevel, ErrorF("Wacom converted x=%d y=%d"
		" to v0=%d v1=%d v2=%d v3=%d v4=%d v5=%d\n", x, y,
		valuators[0], valuators[1], valuators[2], 
		valuators[3], valuators[4], valuators[5]));

	return TRUE;
}
