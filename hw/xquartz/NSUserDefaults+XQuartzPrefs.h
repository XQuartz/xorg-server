/* Copyright (c) 2021 Apple Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 * HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above
 * copyright holders shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization.
 */

#import <Foundation/Foundation.h>

extern NSString * const XQuartzPrefKeyAppsMenu;
extern NSString * const XQuartzPrefKeyFakeButtons;
extern NSString * const XQuartzPrefKeyFakeButton2;
extern NSString * const XQuartzPrefKeyFakeButton3;
extern NSString * const XQuartzPrefKeyKeyEquivs;
extern NSString * const XQuartzPrefKeyFullscreenHotkeys;
extern NSString * const XQuartzPrefKeyFullscreenMenu;
extern NSString * const XQuartzPrefKeySyncKeymap;
extern NSString * const XQuartzPrefKeyDepth;
extern NSString * const XQuartzPrefKeyNoAuth;
extern NSString * const XQuartzPrefKeyNoTCP;
extern NSString * const XQuartzPrefKeyDoneXinitCheck;
extern NSString * const XQuartzPrefKeyNoQuitAlert;
extern NSString * const XQuartzPrefKeyNoRANDRAlert;
extern NSString * const XQuartzPrefKeyOptionSendsAlt;
extern NSString * const XQuartzPrefKeyAppKitModifiers;
extern NSString * const XQuartzPrefKeyWindowItemModifiers;
extern NSString * const XQuartzPrefKeyRootless;
extern NSString * const XQuartzPrefKeyRENDERExtension;
extern NSString * const XQuartzPrefKeyTESTExtension;
extern NSString * const XQuartzPrefKeyLoginShell;
extern NSString * const XQuartzPrefKeyClickThrough;
extern NSString * const XQuartzPrefKeyFocusFollowsMouse;
extern NSString * const XQuartzPrefKeyFocusOnNewWindow;

extern NSString * const XQuartzPrefKeyScrollInDeviceDirection;
extern NSString * const XQuartzPrefKeySyncPasteboard;
extern NSString * const XQuartzPrefKeySyncPasteboardToClipboard;
extern NSString * const XQuartzPrefKeySyncPasteboardToPrimary;
extern NSString * const XQuartzPrefKeySyncClipboardToPasteBoard;
extern NSString * const XQuartzPrefKeySyncPrimaryOnSelect;

@interface NSUserDefaults (XQuartzPrefs)

+ (NSUserDefaults *)xquartzDefaults;

@end
