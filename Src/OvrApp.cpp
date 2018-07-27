/************************************************************************************

Filename    :   OvrApp.cpp
Content     :   Trivial use of the application framework.
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OvrApp.h"
#include "GuiSys.h"
#include "OVR_Locale.h"

using namespace OVR;

#if defined( OVR_OS_ANDROID )
extern "C" {


jlong Java_com_nintendont_virtualboygo_MainActivity_nativeSetAppInterface( JNIEnv * jni, jclass clazz, jobject activity,
		jstring fromPackageName, jstring commandString, jstring uriString )
{
	LOG( "nativeSetAppInterface" );
	return (new OvrApp())->SetActivity( jni, clazz, activity, fromPackageName, commandString, uriString );
}

} // extern "C"
#endif

OvrApp::OvrApp()
	: SoundEffectContext( NULL )
	, SoundEffectPlayer( NULL )
	, GuiSys( OvrGuiSys::Create() )
	, Locale( NULL )
	, SceneModel( NULL )
{
}

OvrApp::~OvrApp()
{
	delete SoundEffectPlayer;
	SoundEffectPlayer = NULL;

	delete SoundEffectContext;
	SoundEffectContext = NULL;

	OvrGuiSys::Destroy( GuiSys );
	if ( SceneModel != NULL )
	{
		delete SceneModel;
	}
}

void OvrApp::Configure( ovrSettings & settings )
{
	settings.CpuLevel = 2;
	settings.GpuLevel = 2;
	settings.RenderMode = RENDERMODE_MULTIVIEW;
}

void OvrApp::EnteredVrMode( const ovrIntentType intentType, const char * intentFromPackage, const char * intentJSON, const char * intentURI )
{
	OVR_UNUSED( intentFromPackage );
	OVR_UNUSED( intentJSON );
	OVR_UNUSED( intentURI );

	if ( intentType == INTENT_LAUNCH )
	{
		const ovrJava * java = app->GetJava();
		SoundEffectContext = new ovrSoundEffectContext( *java->Env, java->ActivityObject );
		SoundEffectContext->Initialize( &app->GetFileSys() );
		SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

		Locale = ovrLocale::Create( *java->Env, java->ActivityObject, "default" );

		String fontName;
		GetLocale().GetString( "@string/font_name", "efigs.fnt", fontName );
		GuiSys->Init( this->app, *SoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines() );

		MaterialParms materialParms;
		materialParms.UseSrgbTextureFormats = false;

		const char * sceneUri = "apk:///assets/box.ovrscene";

		SceneModel = LoadModelFile( app->GetFileSys(), sceneUri, Scene.GetDefaultGLPrograms(), materialParms );

		if ( SceneModel != NULL )
		{
			Scene.SetWorldModel( *SceneModel );
			Scene.SetYawOffset( -MATH_FLOAT_PI * 0.5f );
		}
		else
		{
			LOG( "OvrApp::EnteredVrMode Failed to load %s", sceneUri );
		}
	}
	else if ( intentType == INTENT_NEW )
	{
	}
}

void OvrApp::LeavingVrMode()
{
}

bool OvrApp::OnKeyEvent( const int keyCode, const int repeatCount, const KeyEventType eventType )
{
	if ( GuiSys->OnKeyEvent( keyCode, repeatCount, eventType ) )
	{
		return true;
	}
	return false;
}

ovrFrameResult OvrApp::Frame( const ovrFrameInput & vrFrame )
{
	// process input events first because this mirrors the behavior when OnKeyEvent was
	// a virtual function on VrAppInterface and was called by VrAppFramework.
	for ( int i = 0; i < vrFrame.Input.NumKeyEvents; i++ )
	{
		const int keyCode = vrFrame.Input.KeyEvents[i].KeyCode;
		const int repeatCount = vrFrame.Input.KeyEvents[i].RepeatCount;
		const KeyEventType eventType = vrFrame.Input.KeyEvents[i].EventType;

		if ( OnKeyEvent( keyCode, repeatCount, eventType ) )
		{
			continue;   // consumed the event
		}
		// If nothing consumed the key and it's a short-press of the back key, then exit the application to OculusHome.
		if ( keyCode == OVR_KEY_BACK && eventType == KEY_EVENT_SHORT_PRESS )
		{
			app->ShowSystemUI( VRAPI_SYS_UI_CONFIRM_QUIT_MENU );
			continue;
		}
	}

	// Player movement.
	Scene.Frame( vrFrame );

	ovrFrameResult res;
	Scene.GetFrameMatrices( vrFrame.FovX, vrFrame.FovY, res.FrameMatrices );
	Scene.GenerateFrameSurfaceList( res.FrameMatrices, res.Surfaces );

	// Update GUI systems after the app frame, but before rendering anything.
	GuiSys->Frame( vrFrame, res.FrameMatrices.CenterView );
	// Append GuiSys surfaces.
	GuiSys->AppendSurfaceList( res.FrameMatrices.CenterView, &res.Surfaces );

	res.FrameIndex = vrFrame.FrameNumber;
	res.DisplayTime = vrFrame.PredictedDisplayTimeInSeconds;
	res.SwapInterval = app->GetSwapInterval();

	res.FrameFlags = 0;
	res.LayerCount = 0;

	ovrLayerProjection2 & worldLayer = res.Layers[ res.LayerCount++ ].Projection;
	worldLayer = vrapi_DefaultLayerProjection2();

	worldLayer.HeadPose = vrFrame.Tracking.HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		worldLayer.Textures[eye].ColorSwapChain = vrFrame.ColorTextureSwapChain[eye];
		worldLayer.Textures[eye].SwapChainIndex = vrFrame.TextureSwapChainIndex;
		worldLayer.Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
	}
	worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	return res;
}
