import Combine

/// Live system facts the windows react to. Refreshed by a timer in the app delegate.
@MainActor
final class AppState: ObservableObject {
    @Published var isTrusted = Permissions.isTrusted
    @Published var secureInputIsOn = Permissions.secureInputIsOn

    func refresh() {
        let trusted = Permissions.isTrusted
        if trusted != isTrusted { isTrusted = trusted }
        let secure = Permissions.secureInputIsOn
        if secure != secureInputIsOn { secureInputIsOn = secure }
    }
}
