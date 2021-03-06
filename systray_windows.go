package systray

import (
	"strconv"
	//	"fmt"
	"io/ioutil"
	"os"
	//	"path/filepath"
	"syscall"
	"unsafe"

	//	"github.com/getlantern/filepersist"
)

var (
	iconFiles = make([]*os.File, 0)
	dllFile   = "systray.dll"

	mod                                = syscall.NewLazyDLL(dllFile)
	_nativeLoop                        = mod.NewProc("nativeLoop")
	_quit                              = mod.NewProc("quit")
	_setIcon                           = mod.NewProc("setIcon")
	_setTitle                          = mod.NewProc("setTitle")
	_setTooltip                        = mod.NewProc("setTooltip")
	_add_or_update_menu_item           = mod.NewProc("add_or_update_menu_item")
	_add_or_update_menu_item_with_icon = mod.NewProc("add_or_update_menu_item_with_icon")
)

func nativeLoop() {
	_nativeLoop.Call(
		syscall.NewCallbackCDecl(systray_ready),
		syscall.NewCallbackCDecl(systray_menu_item_selected),
		syscall.NewCallbackCDecl(systray_leftmouse_clicked))
}

func quit() {
	_quit.Call()
	for _, f := range iconFiles {
		err := os.Remove(f.Name())
		if err != nil {
			log.Debugf("Unable to delete temporary icon file %v: %v", f.Name(), err)
		}
	}
}

// SetIcon sets the systray icon.
// iconBytes should be the content of .ico for windows and .ico/.jpg/.png
// for other platforms.
func SetIcon(iconBytes []byte) {
	f, err := ioutil.TempFile("", "systray_temp_icon")
	if err != nil {
		log.Errorf("Unable to create temp icon: %v", err)
		return
	}
	defer f.Close()
	_, err = f.Write(iconBytes)
	if err != nil {
		log.Errorf("Unable to write icon to temp file %v: %v", f.Name(), f)
		return
	}
	// Need to close file before we load it to make sure contents is flushed.
	f.Close()
	name, err := strUTF16(f.Name())
	if err != nil {
		log.Errorf("Unable to convert name to string pointer: %v", err)
		return
	}
	_setIcon.Call(name.Raw())
}

func SetFileIcon(filePath string) {
	name, err := strUTF16(filePath)
	if err != nil {
		log.Errorf("Unable to convert name to string pointer: %v", err)
		return
	}
	_setIcon.Call(name.Raw())
}

// SetTitle sets the systray title, only available on Mac.
func SetTitle(title string) {
	// do nothing
}

// SetTooltip sets the systray tooltip to display on mouse hover of the tray icon,
// only available on Mac and Windows.
func SetTooltip(tooltip string) {
	t, err := strUTF16(tooltip)
	if err != nil {
		log.Errorf("Unable to convert tooltip to string pointer: %v", err)
		return
	}
	_setTooltip.Call(t.Raw())
}

func addOrUpdateMenuItem(item *MenuItem) {
	var disabled = 0
	if item.disabled {
		disabled = 1
	}
	var checked = 0
	if item.checked {
		checked = 1
	}
	title, err := strUTF16(item.title)
	if err != nil {
		log.Errorf("Unable to convert title to string pointer: %v", err)
		return
	}
	tooltip, err := strUTF16(item.tooltip)
	if err != nil {
		log.Errorf("Unable to convert tooltip to string pointer: %v", err)
		return
	}
	_add_or_update_menu_item.Call(
		uintptr(item.id),
		title.Raw(),
		tooltip.Raw(),
		uintptr(disabled),
		uintptr(checked),
	)
}

func addOrUpdateMenuItemWithIcon(item *MenuItem, data []byte) {

	f, err := ioutil.TempFile("", "systray_temp_item_icon"+strconv.Itoa(int(item.id)))
	if err != nil {
		log.Errorf("Unable to create temp icon: %v", err)
		return
	}
	defer f.Close()
	_, err = f.Write(data)
	if err != nil {
		log.Errorf("Unable to write icon to temp file %v: %v", f.Name(), f)
		return
	}
	// Need to close file before we load it to make sure contents is flushed.
	f.Close()

	addOrUpdateMenuItemWithFileIcon(item, f.Name())
}

func addOrUpdateMenuItemWithFileIcon(item *MenuItem, filePath string) {
	var disabled = 0
	if item.disabled {
		disabled = 1
	}
	var checked = 0
	if item.checked {
		checked = 1
	}
	title, err := strUTF16(item.title)
	if err != nil {
		log.Errorf("Unable to convert title to string pointer: %v", err)
		return
	}
	tooltip, err := strUTF16(item.tooltip)
	if err != nil {
		log.Errorf("Unable to convert tooltip to string pointer: %v", err)
		return
	}
	name, err := strUTF16(filePath)
	if err != nil {
		log.Errorf("Unable to convert filePath to string pointer: %v", err)
		return
	}

	log.Debug("_add_or_update_menu_item_with_icon call")
	_add_or_update_menu_item_with_icon.Call(
		uintptr(item.id),
		title.Raw(),
		tooltip.Raw(),
		name.Raw(),
		uintptr(disabled),
		uintptr(checked),
	)
}

type utf16 []uint16

// Raw returns the underlying *wchar_t of an utf16 so we can pass to DLL
func (u utf16) Raw() uintptr {
	return uintptr(unsafe.Pointer(&u[0]))
}

// strUTF16 converts a Go string into a utf16 byte sequence
func strUTF16(s string) (utf16, error) {
	return syscall.UTF16FromString(s)
}

// systray_ready takes an ignored parameter just so we can compile a callback
// (for some reason in Go 1.4.x, syscall.NewCallback panics if there's no
// parameter to the function).
func systray_ready(ignore uintptr) uintptr {
	systrayReady()
	return 0
}

func systray_menu_item_selected(id uintptr) uintptr {
	systrayMenuItemSelected(int32(id))
	return 0
}

func systray_leftmouse_clicked() uintptr {
	systrayLeftMouseClicked()
	return 0
}
