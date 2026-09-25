// The demo / device check (the Android demo's twin): bundles tests/golden, runs every golden through
// the core on the device, compares the JSON with the expected files, and paints each draw list on
// screen with the CoreGraphics painter.
import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        window.rootViewController = GoldenViewController()
        window.makeKeyAndVisible()
        self.window = window
        return true
    }
}
