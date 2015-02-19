/*
 Copyright (c) 2015, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#include "cinder/app/cocoa/PlatformCocoa.h"
#include "cinder/app/AppBase.h"
#include "cinder/Log.h"
#include "cinder/Filesystem.h"

#if defined( CINDER_MAC )
	#import <Cocoa/Cocoa.h>
#else
	#import <Foundation/Foundation.h>
	#import <UIKit/UIKit.h>
#endif

using namespace std;

namespace cinder { namespace app {

PlatformCocoa::PlatformCocoa()
	: mBundle( nil ), mDisplaysInitialized( false )
{
	// This is necessary to force the linker not to strip these symbols from libboost_filesystem.a,
	// which in turn would force users to explicitly link to that lib from their own apps.
	auto dummy = boost::filesystem::unique_path();
	auto dummy2 = fs::temp_directory_path();
}

void PlatformCocoa::prepareLaunch()
{
	mAutoReleasePool = [[NSAutoreleasePool alloc] init];
}

void PlatformCocoa::cleanupLaunch()
{
	[mAutoReleasePool drain];
}

void PlatformCocoa::setBundle( NSBundle *bundle )
{
	mBundle = bundle;
}

NSBundle* PlatformCocoa::getBundle() const
{
	if( ! mBundle )
		mBundle = [NSBundle mainBundle];

	return mBundle;
}

fs::path PlatformCocoa::getResourcePath( const fs::path &rsrcRelativePath ) const
{
	fs::path path = rsrcRelativePath.parent_path();
	fs::path fileName = rsrcRelativePath.filename();

	if( fileName.empty() )
		return fs::path();

	NSString *pathNS = NULL;
	if( ( ! path.empty() ) && ( path != rsrcRelativePath ) )
		pathNS = [NSString stringWithUTF8String:path.c_str()];

	NSString *resultPath = [getBundle() pathForResource:[NSString stringWithUTF8String:fileName.c_str()] ofType:NULL inDirectory:pathNS];
	if( ! resultPath )
		return fs::path();

	return fs::path( [resultPath cStringUsingEncoding:NSUTF8StringEncoding] );
}

fs::path PlatformCocoa::getResourcePath() const
{
	NSString *resultPath = [getBundle() resourcePath];

	return fs::path( [resultPath cStringUsingEncoding:NSUTF8StringEncoding] );
}

DataSourceRef PlatformCocoa::loadResource( const fs::path &resourcePath )
{
	fs::path fullPath = getResourcePath( resourcePath );
	if( fullPath.empty() )
		throw ResourceLoadExc( resourcePath );
	else
		return DataSourcePath::create( fullPath );
}

void PlatformCocoa::prepareAssetLoading()
{
	// search for the assets folder inside the bundle's resources, and then the bundle's root
	fs::path bundleAssetsPath = getResourcePath() / "assets";
	if( fs::exists( bundleAssetsPath ) && fs::is_directory( bundleAssetsPath ) ) {
		addAssetDirectory( bundleAssetsPath );
	}
	else {
		fs::path appAssetsPath = getExecutablePath() / "assets";
		if( fs::exists( appAssetsPath ) && fs::is_directory( appAssetsPath ) ) {
			addAssetDirectory( appAssetsPath );
		}
	}
}

fs::path PlatformCocoa::getOpenFilePath( const fs::path &initialPath, const vector<string> &extensions )
{
#if defined( CINDER_MAC )
	NSOpenPanel *cinderOpen = [NSOpenPanel openPanel];
	[cinderOpen setCanChooseFiles:YES];
	[cinderOpen setCanChooseDirectories:NO];
	[cinderOpen setAllowsMultipleSelection:NO];

	if( ! extensions.empty() ) {
		NSMutableArray *typesArray = [NSMutableArray arrayWithCapacity:extensions.size()];
		for( const auto &ext : extensions )
			[typesArray addObject:[NSString stringWithUTF8String:ext.c_str()]];

		[cinderOpen setAllowedFileTypes:typesArray];
	}

	if( ! initialPath.empty() )
		[cinderOpen setDirectoryURL:[NSURL fileURLWithPath:[[NSString stringWithUTF8String:initialPath.c_str()] stringByExpandingTildeInPath]]];

	NSInteger resultCode = [cinderOpen runModal];

	if( resultCode == NSFileHandlingPanelOKButton ) {
		NSString *result = [[[cinderOpen URLs] firstObject] path];
		if( ! result ) {
			CI_LOG_E( "empty path result" );
			return fs::path();
		}
		else
			return fs::path( [result UTF8String] );
	}
#endif // defined( CINDER_MAC )

	return fs::path(); // return empty path on failure
}

fs::path PlatformCocoa::getFolderPath( const fs::path &initialPath )
{
#if defined( CINDER_MAC )
	NSOpenPanel *cinderOpen = [NSOpenPanel openPanel];
	[cinderOpen setCanChooseFiles:NO];
	[cinderOpen setCanChooseDirectories:YES];
	[cinderOpen setAllowsMultipleSelection:NO];

	if( ! initialPath.empty() )
		[cinderOpen setDirectoryURL:[NSURL fileURLWithPath:[[NSString stringWithUTF8String:initialPath.c_str()] stringByExpandingTildeInPath]]];

	NSInteger resultCode = [cinderOpen runModal];

	if( resultCode == NSFileHandlingPanelOKButton ) {
		NSString *result = [[[cinderOpen URLs] firstObject] path];
		if( ! result ) {
			CI_LOG_E( "empty path result" );
			return fs::path();
		}
		else
			return fs::path( [result UTF8String] );
	}
#endif // defined( CINDER_MAC )

	return fs::path(); // return empty path on failure
}

fs::path PlatformCocoa::getSaveFilePath( const fs::path &initialPath, const vector<string> &extensions )
{
#if defined( CINDER_MAC )
	NSSavePanel *cinderSave = [NSSavePanel savePanel];

	NSMutableArray *typesArray = nil;
	if( ! extensions.empty() ) {
		typesArray = [NSMutableArray arrayWithCapacity:extensions.size()];
		for( const auto &ext : extensions )
			[typesArray addObject:[NSString stringWithUTF8String:ext.c_str()]];

		[cinderSave setAllowedFileTypes:typesArray];
	}

	if( ! initialPath.empty() ) {
		NSString *directory, *file = nil;
		directory = [[NSString stringWithUTF8String:initialPath.c_str()] stringByExpandingTildeInPath];
		BOOL isDir;
		if( [[NSFileManager defaultManager] fileExistsAtPath:directory isDirectory:&isDir] ) {
			if( ! isDir ) { // a file exists at this path, so directory is its parent
				file = [directory lastPathComponent];
				directory = [directory stringByDeletingLastPathComponent];
			}
		}
		else {
			file = [directory lastPathComponent];
			directory = [directory stringByDeletingLastPathComponent];
		}

		[cinderSave setDirectoryURL:[NSURL fileURLWithPath:directory]];
		if( file )
			[cinderSave setNameFieldStringValue:file];
	}

	NSInteger resultCode = [cinderSave runModal];

	if( resultCode == NSFileHandlingPanelOKButton ) {
		return fs::path( [[[cinderSave URL] path] UTF8String] );
	}
#endif // defined( CINDER_MAC )

	return fs::path(); // return empty path on failure
}

void PlatformCocoa::addDisplay( const DisplayRef &display )
{
	mDisplays.push_back( display );

	if( app::AppBase::get() )
		app::AppBase::get()->emitDisplayConnected( display );
}

void PlatformCocoa::removeDisplay( const DisplayRef &display )
{
	DisplayRef displayCopy = display;
	mDisplays.erase( std::remove( mDisplays.begin(), mDisplays.end(), displayCopy ), mDisplays.end() );
	
	if( app::AppBase::get() )
		app::AppBase::get()->emitDisplayDisconnected( displayCopy );
}

} // namespace app

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DisplayMac
#if defined( CINDER_MAC )
DisplayMac::~DisplayMac()
{
	[mScreen release];
}

DisplayRef app::PlatformCocoa::findFromCgDirectDisplayId( CGDirectDisplayID displayId )
{
	for( vector<DisplayRef>::iterator dispIt = mDisplays.begin(); dispIt != mDisplays.end(); ++dispIt ) {
		const DisplayMac& macDisplay( dynamic_cast<const DisplayMac&>( **dispIt ) );
		if( macDisplay.getCgDirectDisplayId() == displayId )
			return *dispIt;
	}

	// couldn't find it, so return nullptr
	return nullptr;
}

namespace {
NSScreen* findNsScreenForCgDirectDisplayId( CGDirectDisplayID displayId )
{
	NSArray *screens = [NSScreen screens];
	size_t screenCount = [screens count];
	for( size_t i = 0; i < screenCount; ++i ) {
		::NSScreen *screen = [screens objectAtIndex:i];
		CGDirectDisplayID thisId = (CGDirectDisplayID)[[[screen deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
		if( thisId == displayId )
			return screen;
	}
	
	return nil;
}
} // anonymous namespace

DisplayRef app::PlatformCocoa::findFromNsScreen( NSScreen *nsScreen )
{
	return findFromCgDirectDisplayId( (CGDirectDisplayID)[[[nsScreen deviceDescription] objectForKey:@"NSScreenNumber"] intValue] );
}

void DisplayMac::displayReconfiguredCallback( CGDirectDisplayID displayId, CGDisplayChangeSummaryFlags flags, void *userInfo )
{
	if( flags & kCGDisplayRemoveFlag ) {
		DisplayRef display = app::PlatformCocoa::get()->findFromCgDirectDisplayId( displayId );
		if( display )
			app::PlatformCocoa::get()->removeDisplay( display ); // this will signal
		else
			CI_LOG_W( "Received removed from CGDisplayRegisterReconfigurationCallback() on unknown display" );		
	}
	else if( flags & kCGDisplayAddFlag ) {
		DisplayRef display = app::PlatformCocoa::get()->findFromCgDirectDisplayId( displayId );
		if( ! display ) {
			CGRect frame = ::CGDisplayBounds( displayId );
			
			DisplayMac *newDisplay = new DisplayMac();
			newDisplay->mDirectDisplayID = displayId;
			newDisplay->mArea = Area( frame.origin.x, frame.origin.y, frame.origin.x + frame.size.width, frame.origin.y + frame.size.height );
			newDisplay->mScreen = findNsScreenForCgDirectDisplayId( displayId );			
			if( newDisplay->mScreen ) {
				newDisplay->mContentScale = [newDisplay->mScreen backingScaleFactor];
				newDisplay->mBitsPerPixel = (int)NSBitsPerPixelFromDepth( [newDisplay->mScreen depth] );
			}
			else {
				newDisplay->mContentScale = 1.0f;
				newDisplay->mBitsPerPixel = 24;
				CI_LOG_E( "Unable to locate corresponding NSScreen for CGDirectDisplayID" );
			}
			
			app::PlatformCocoa::get()->addDisplay( DisplayRef( newDisplay ) ); // this will signal
		}
		else
			CI_LOG_W( "Received add from CGDisplayRegisterReconfigurationCallback() for already known display" );				
	}
	else if( flags & kCGDisplayMovedFlag ) { // needs to be tested after add & remove
		DisplayRef display = app::PlatformCocoa::get()->findFromCgDirectDisplayId( displayId );
		if( display ) {
			bool newMainDisplay = false;
			if( flags & kCGDisplaySetMainFlag ) {
				DisplayRef display = app::PlatformCocoa::get()->findFromCgDirectDisplayId( displayId );
				if( display && ( display != app::PlatformCocoa::get()->getDisplays()[0] ) ) {
					newMainDisplay = true;
					// move on up to the front of the bus; mDisplays[0] is main
					vector<DisplayRef> &mDisplays = app::PlatformCocoa::get()->mDisplays;
					mDisplays.erase( std::remove( mDisplays.begin(), mDisplays.end(), display ), mDisplays.end() );
					mDisplays.insert( mDisplays.begin(), display );
				}
			}		
		
			// CG appears to not do the coordinate y-flip that NSScreen does
			CGRect frame = ::CGDisplayBounds( displayId );
			Area displayArea( frame.origin.x, frame.origin.y, frame.origin.x + frame.size.width, frame.origin.y + frame.size.height );
			bool newArea = false;
			if( display->getBounds() != displayArea ) {
				reinterpret_cast<DisplayMac*>( display.get() )->mArea = displayArea;
				newArea = true;
			}
			
			if( newMainDisplay || newArea ) {
				if( app::AppBase::get() )
					app::AppBase::get()->emitDisplayChanged( display );
			}
		}
		else
			CI_LOG_W( "Received moved from CGDisplayRegisterReconfigurationCallback() on unknown display" );			
	}
}

const std::vector<DisplayRef>& app::PlatformCocoa::getDisplays( bool forceRefresh )
{
	if( ! mDisplaysInitialized ) {
		// this is our first call; register a callback with CoreGraphics for any display changes. Note that this only works with a run loop
		::CGDisplayRegisterReconfigurationCallback( DisplayMac::displayReconfiguredCallback, NULL );
	}

	if( forceRefresh || ( ! mDisplaysInitialized ) ) {
		NSArray *screens = [NSScreen screens];
		NSScreen *mainScreen = [screens objectAtIndex:0];
		size_t screenCount = [screens count];
		for( size_t i = 0; i < screenCount; ++i ) {
			::NSScreen *screen = [screens objectAtIndex:i];
			[screen retain]; // this is released in the destructor for Display

			DisplayMac *newDisplay = new DisplayMac();

			NSRect frame = [screen frame];
			// The Mac measures screens relative to the lower-left corner of the primary display. We need to correct for this
			if( screen != mainScreen ) {
				int mainScreenHeight = (int)[mainScreen frame].size.height;
				newDisplay->mArea = Area( frame.origin.x, mainScreenHeight - frame.origin.y - frame.size.height, frame.origin.x + frame.size.width, mainScreenHeight - frame.origin.y );
			}
			else
				newDisplay->mArea = Area( frame.origin.x, frame.origin.y, frame.origin.x + frame.size.width, frame.origin.y + frame.size.height );

			newDisplay->mDirectDisplayID = (CGDirectDisplayID)[[[screen deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
			newDisplay->mScreen = screen;
			newDisplay->mBitsPerPixel = (int)NSBitsPerPixelFromDepth( [screen depth] );
			newDisplay->mContentScale = [screen backingScaleFactor];

			mDisplays.push_back( DisplayRef( newDisplay ) );
		}

		mDisplaysInitialized = true;	
	}
	
	return mDisplays;
}

#else // COCOA_TOUCH

DisplayCocoaTouch::~DisplayCocoaTouch()
{
	[mUiScreen release];
}

const std::vector<DisplayRef>& app::PlatformCocoa::getDisplays( bool forceRefresh )
{
	if( forceRefresh || ( ! mDisplaysInitialized ) ) {
		NSArray *screens = [UIScreen screens];
		for( UIScreen *screen in screens ) {
			[screen retain]; // this is released in the destructor for Display
			CGRect frame = [screen bounds];

			DisplayCocoaTouch *newDisplay = new DisplayCocoaTouch();
			newDisplay->mArea = Area( frame.origin.x, frame.origin.y, frame.origin.x + frame.size.width, frame.origin.y + frame.size.height );
			newDisplay->mUiScreen = screen;
			newDisplay->mBitsPerPixel = 24;
			newDisplay->mContentScale = screen.scale;

			NSArray *resolutions = [screen availableModes];
			for( int i = 0; i < [resolutions count]; ++i ) {
				::UIScreenMode *mode = [resolutions objectAtIndex:i];
				newDisplay->mSupportedResolutions.push_back( ivec2( (int32_t)mode.size.width, (int32_t)mode.size.height ) );
			}

			mDisplays.push_back( DisplayRef( newDisplay ) );
		}

		// <TEMPORARY>
		// This is a workaround for a beta of iOS 8 SDK, which appears to return an empty array for screens
		if( [screens count] == 0 ) {
			UIScreen *screen = [UIScreen mainScreen];
			[screen retain];
			CGRect frame = [screen bounds];

			DisplayCocoaTouch *newDisplay = new DisplayCocoaTouch();
			newDisplay->mArea = Area( frame.origin.x, frame.origin.y, frame.origin.x + frame.size.width, frame.origin.y + frame.size.height );
			newDisplay->mUiScreen = screen;
			newDisplay->mBitsPerPixel = 24;
			newDisplay->mContentScale = screen.scale;

			NSArray *modes = [screen availableModes];
			for( UIScreenMode *mode in modes ) {
				newDisplay->mSupportedResolutions.push_back( ivec2( (int32_t)mode.size.width, (int32_t)mode.size.height ) );
			}

			mDisplays.push_back( DisplayRef( newDisplay ) );
		}
		// </TEMPORARY>

		mDisplaysInitialized = true;
	}
	
	return mDisplays;
}

void DisplayCocoaTouch::setResolution( const ivec2 &resolution )
{
	NSArray *modes = [mUiScreen availableModes];
	int closestIndex = 0;
	float closestDistance = 1000000.0f; // big distance
	for( int i = 0; i < [modes count]; ++i ) {
		::UIScreenMode *mode = [modes objectAtIndex:i];
		ivec2 thisModeRes = vec2( mode.size.width, mode.size.height );
		if( distance( vec2(resolution), vec2(thisModeRes) ) < closestDistance ) {
			closestDistance = distance( vec2(resolution), vec2(thisModeRes) );
			closestIndex = i;
		}
	}
	
	mUiScreen.currentMode = [modes objectAtIndex:closestIndex];
}
#endif

} // namespace cinder
