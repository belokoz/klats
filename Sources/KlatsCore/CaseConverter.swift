public enum CaseConverter {
    public enum Mode: String, Sendable, CaseIterable {
        /// Every letter flips: «пРИВЕТ» → «Привет». Fixes text typed with Caps Lock on.
        case invert
        /// Everything becomes lowercase: «ПРИВЕТ» → «привет».
        case lowercase
    }

    public static func convert(_ text: String, mode: Mode) -> String {
        switch mode {
        case .lowercase:
            return text.lowercased()
        case .invert:
            var result = ""
            result.reserveCapacity(text.utf8.count)
            for character in text {
                if character.isLowercase {
                    result += character.uppercased()
                } else if character.isUppercase {
                    result += character.lowercased()
                } else {
                    result.append(character)
                }
            }
            return result
        }
    }
}
