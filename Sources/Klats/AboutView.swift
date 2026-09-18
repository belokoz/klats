import SwiftUI

/// «О программе»: the same content and link style as the settings footer.
struct AboutView: View {
    private static let build = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "0"

    var body: some View {
        VStack(spacing: 6) {
            Image(nsImage: AppIcon.image(size: 96))
                .resizable().frame(width: 96, height: 96)
                .padding(.bottom, 6)
            Text("Клац").font(.title2.weight(.semibold))
            Text("\(L("Версия")) \(Links.version) (\(Self.build))")
                .font(.caption).foregroundStyle(.secondary)
            Text(L("Исправляет раскладку выделенного текста одним нажатием."))
                .font(.callout).foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .padding(.top, 10)
            VStack(spacing: 7) {
                Link("github.com/belokoz/klats", destination: Links.repo)
                    .pointingHandOnHover()
                Link(destination: Links.diktuy(from: "about")) {
                    HStack(spacing: 5) {
                        Image(systemName: "mic.fill")
                        Text(L("Диктуй")).fontWeight(.semibold) + Text(L(": голос в текст"))
                    }
                }
                .pointingHandOnHover()
            }
            .font(.callout)
            .padding(.top, 12)
            Text("MIT License")
                .font(.caption).foregroundStyle(.secondary)
                .padding(.top, 12)
        }
        .padding(.horizontal, 28)
        .padding(.top, 8)
        .padding(.bottom, 24)
        .frame(width: 320)
        .fixedSize(horizontal: false, vertical: true)
    }
}
