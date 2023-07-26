LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include ../../../../cflags.mk

LOCAL_MODULE	:= sampleframework

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb
LOCAL_ARM_NEON  := true				# compile with neon support enabled

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../Src \
					$(LOCAL_PATH)/../../../../../VrApi/Include \
  					$(LOCAL_PATH)/../../../../../1stParty/OVR/Include \
  					$(LOCAL_PATH)/../../../../../1stParty/utilities/include \
  					$(LOCAL_PATH)/../../../../../3rdParty/stb/src \

LOCAL_CFLAGS += -Wno-invalid-offsetof

LOCAL_SRC_FILES		:= 	../../../Src/Appl.cpp \
						../../../Src/System.cpp \
						../../../Src/PackageFiles.cpp \
						../../../Src/OVR_UTF8Util.cpp \
						../../../Src/OVR_Uri.cpp \
						../../../Src/OVR_FileSys.cpp \
						../../../Src/OVR_Stream.cpp \
						../../../Src/OVR_MappedFile.cpp \
						../../../Src/OVR_BinaryFile2.cpp \
						../../../Src/OVR_Lexer2.cpp \
						../../../Src/MessageQueue.cpp \
						../../../Src/TalkToJava.cpp \
						../../../Src/Platform/Android/Android.cpp \
						../../../Src/Misc/Log.c \
						../../../Src/Render/Egl.c \
						../../../Src/Render/Framebuffer.c \
						../../../Src/Render/GlSetup.cpp \
						../../../Src/Render/GlBuffer.cpp \
						../../../Src/Render/GlTexture.cpp \
						../../../Src/Render/GlProgram.cpp \
						../../../Src/Render/GlGeometry.cpp \
						../../../Src/Render/SurfaceRender.cpp \
						../../../Src/Render/SurfaceTexture.cpp \
						../../../Src/Render/BitmapFont.cpp \
						../../../Src/Render/DebugLines.cpp \
						../../../Src/Render/TextureManager.cpp \
						../../../Src/Render/EaseFunctions.cpp \
						../../../Src/Render/TextureAtlas.cpp \
						../../../Src/Render/ParticleSystem.cpp	\
						../../../Src/Render/PointList.cpp \
						../../../Src/Render/Ribbon.cpp \
						../../../Src/Render/BeamRenderer.cpp \
						../../../Src/Render/PanelRenderer.cpp \
						../../../Src/Model/ModelCollision.cpp \
						../../../Src/Model/ModelTrace.cpp \
						../../../Src/Model/ModelRender.cpp \
						../../../Src/Model/ModelFile.cpp \
						../../../Src/Model/ModelFile_OvrScene.cpp \
						../../../Src/Model/ModelFile_glTF.cpp \
						../../../Src/Model/SceneView.cpp \
 						../../../Src/Locale/tinyxml2.cpp \
 						../../../Src/Locale/OVR_Locale.cpp \
 						../../../Src/GUI/CollisionPrimitive.cpp \
 						../../../Src/GUI/GazeCursor.cpp \
 						../../../Src/GUI/VRMenu.cpp \
 						../../../Src/GUI/SoundLimiter.cpp \
 						../../../Src/GUI/VRMenuObject.cpp \
 						../../../Src/GUI/VRMenuMgr.cpp \
 						../../../Src/GUI/VRMenuComponent.cpp \
 						../../../Src/GUI/VRMenuEvent.cpp \
 						../../../Src/GUI/VRMenuEventHandler.cpp \
 						../../../Src/GUI/Reflection.cpp \
 						../../../Src/GUI/AnimComponents.cpp \
 						../../../Src/GUI/Fader.cpp \
 						../../../Src/GUI/DefaultComponent.cpp \
 						../../../Src/GUI/ReflectionData.cpp \
 						../../../Src/GUI/ActionComponents.cpp \
 						../../../Src/GUI/MetaDataManager.cpp \
 						../../../Src/GUI/ProgressBarComponent.cpp \
 						../../../Src/GUI/ScrollBarComponent.cpp \
 						../../../Src/GUI/SwipeHintComponent.cpp \
 						../../../Src/GUI/TextFade_Component.cpp \
 						../../../Src/GUI/SliderComponent.cpp \
 						../../../Src/GUI/FolderBrowser.cpp \
 						../../../Src/GUI/GuiSys.cpp \
 						../../../Src/GUI/ScrollManager.cpp \
 						../../../Src/GUI/UI/UITexture.cpp \
 	                    ../../../Src/GUI/UI/UIMenu.cpp \
 	                    ../../../Src/GUI/UI/UIObject.cpp \
 	                    ../../../Src/GUI/UI/UIContainer.cpp \
 	                    ../../../Src/GUI/UI/UILabel.cpp \
 	                    ../../../Src/GUI/UI/UIImage.cpp \
 	                    ../../../Src/GUI/UI/UIButton.cpp \
 	                    ../../../Src/GUI/UI/UIProgressBar.cpp \
 	                    ../../../Src/GUI/UI/UINotification.cpp \
 						../../../Src/GUI/UI/UISlider.cpp \
 						../../../Src/GUI/UI/UIDiscreteSlider.cpp \
 						../../../Src/GUI/UI/UIKeyboard.cpp \
 						../../../Src/GUI/UI/UITextBox.cpp \
 						../../../Src/Input/ArmModel.cpp \
 						../../../Src/Input/HandModel.cpp \
 						../../../Src/Input/Skeleton.cpp \
					 	../../../Src/Sound/SoundAssetMapping.cpp \
						../../../Src/Sound/SoundEffectContext.cpp \
						../../../Src/Sound/SoundPool.cpp


LOCAL_STATIC_LIBRARIES += minizip stb android_native_app_glue

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

$(call import-module,android/native_app_glue)
$(call import-module,3rdParty/minizip/build/android/jni)
$(call import-module,3rdParty/stb/build/android/jni)
