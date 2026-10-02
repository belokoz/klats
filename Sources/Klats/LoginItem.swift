import Foundation
import ServiceManagement

/// «Launch at login». On macOS 13 and newer it goes through the system's own list in System
/// Settings. macOS 12 has no API for an app to add itself to its Login Items, so there Klats puts
/// a LaunchAgent into ~/Library/LaunchAgents instead.
@MainActor
enum LoginItem {
    static var isEnabled: Bool {
        if #available(macOS 13, *), !Compat.simulatesMacOS12 {
            // A macOS 12 agent that could not be moved yet still starts Klats at login.
            return SMAppService.mainApp.status == .enabled || LaunchAgent.isInstalled
        }
        return LaunchAgent.isInstalled
    }

    /// The system remembers where the app was when «launch at login» was switched on. After the
    /// app moves (say, from a build folder into Applications) the entry points at the old place,
    /// so an enabled entry is re-registered from wherever the app lives now. The macOS 12 agent
    /// finds the app by its bundle id and needs no refresh.
    static func refreshLocation() {
        guard #available(macOS 13, *), !Compat.simulatesMacOS12 else { return }
        // Switched on under macOS 12, then the Mac was upgraded: move over to the system's list.
        // The agent goes only once the system has taken over, so a failed move loses nothing.
        if LaunchAgent.isInstalled {
            do {
                try SMAppService.mainApp.register()
                guard SMAppService.mainApp.status == .enabled else {
                    Log.write("login item move deferred, status \(SMAppService.mainApp.status.rawValue); keeping the launch agent")
                    return
                }
                LaunchAgent.remove()
                Log.write("login item moved from the macOS 12 launch agent to the system list")
            } catch {
                Log.write("login item move failed, keeping the launch agent: \(error.localizedDescription)")
            }
            return
        }
        guard SMAppService.mainApp.status == .enabled else { return }
        do {
            try SMAppService.mainApp.register()
        } catch {
            Log.write("login item refresh failed: \(error.localizedDescription)")
        }
    }

    /// Returns false when the change did not happen, so a switch can stay where it was.
    @discardableResult
    static func setEnabled(_ enabled: Bool) -> Bool {
        guard #available(macOS 13, *), !Compat.simulatesMacOS12 else {
            return enabled ? LaunchAgent.install() : LaunchAgent.remove()
        }
        do {
            if enabled {
                try SMAppService.mainApp.register()
            } else {
                // Also drop a macOS 12 agent that was never moved, or it would keep starting Klats.
                LaunchAgent.remove()
                try SMAppService.mainApp.unregister()
            }
            return true
        } catch {
            Log.write("login item \(enabled ? "register" : "unregister") failed: \(error.localizedDescription)")
            return false
        }
    }
}

/// macOS 12: a per-user LaunchAgent that runs `open -g -b io.github.belokoz.klats` at login. Launch
/// Services then starts whichever copy of Klats it knows, so moving the app does not break it;
/// `-g` keeps it from taking the keyboard focus. Nothing in System Preferences shows this agent:
/// the switch in Klats is the only way to turn it off, and deleting the app leaves the file behind.
@MainActor
private enum LaunchAgent {
    private static let label = "io.github.belokoz.klats.login"
    private static let bundleID = Bundle.main.bundleIdentifier ?? "io.github.belokoz.klats"

    private static var fileURL: URL {
        FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/LaunchAgents/\(label).plist")
    }

    static var isInstalled: Bool { FileManager.default.fileExists(atPath: fileURL.path) }

    @discardableResult
    static func install() -> Bool {
        let plist: [String: Any] = [
            "Label": label,
            "ProgramArguments": ["/usr/bin/open", "-g", "-b", bundleID],
            "RunAtLoad": true,
            "LimitLoadToSessionType": "Aqua",
            // macOS 13 and newer name the agent after Klats, not after «open», until it is moved.
            "AssociatedBundleIdentifiers": [bundleID],
        ]
        do {
            try FileManager.default.createDirectory(at: fileURL.deletingLastPathComponent(), withIntermediateDirectories: true)
            let data = try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
            try data.write(to: fileURL, options: .atomic)
            return true
        } catch {
            Log.write("launch agent install failed: \(error.localizedDescription)")
            return false
        }
    }

    @discardableResult
    static func remove() -> Bool {
        guard isInstalled else { return true }
        do {
            try FileManager.default.removeItem(at: fileURL)
            return true
        } catch {
            Log.write("launch agent removal failed: \(error.localizedDescription)")
            return false
        }
    }
}
