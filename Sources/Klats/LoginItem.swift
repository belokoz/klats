import ServiceManagement

/// «Launch at login» through the system's own list in System Settings.
@MainActor
enum LoginItem {
    static var isEnabled: Bool { SMAppService.mainApp.status == .enabled }

    /// The system remembers where the app was when «launch at login» was switched on. After the
    /// app moves (say, from a build folder into Applications) the entry points at the old place,
    /// so an enabled entry is re-registered from wherever the app lives now.
    static func refreshLocation() {
        guard isEnabled else { return }
        do {
            try SMAppService.mainApp.register()
        } catch {
            Log.write("login item refresh failed: \(error.localizedDescription)")
        }
    }

    static func setEnabled(_ enabled: Bool) {
        do {
            if enabled { try SMAppService.mainApp.register() } else { try SMAppService.mainApp.unregister() }
        } catch {
            Log.write("login item \(enabled ? "register" : "unregister") failed: \(error.localizedDescription)")
        }
    }
}
