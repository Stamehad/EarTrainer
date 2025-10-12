// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "AppUI",
    defaultLocalization: "en",
    platforms: [
        .iOS(.v16),
        .macOS(.v13)
    ],
    products: [
        .library(
            name: "Bridge",
            targets: ["Bridge"]
        ),
        .library(
            name: "AppUI",
            targets: ["AppUI"]
        )
    ],
    targets: [
        .target(
            name: "CEarTrainerBridge",
            path: "Sources/CEarTrainerBridge",
            publicHeadersPath: "include"
        ),
        .target(
            name: "Bridge",
            dependencies: ["CEarTrainerBridge"],
            path: "Sources/Bridge"
        ),
        .target(
            name: "AppUI",
            dependencies: ["Bridge"],
            path: "Sources/AppUI",
            swiftSettings: [
                .define("DEBUG", .when(configuration: .debug))
            ]
        ),
        .testTarget(
            name: "AppUITests",
            dependencies: ["AppUI"],
            path: "Tests/AppUITests"
        )
    ]
)
