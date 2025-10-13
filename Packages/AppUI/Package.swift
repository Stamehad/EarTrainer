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
            name: "EarTrainerEngine",
            path: "Sources/CEarTrainerBridge/Engine",
            exclude: [
                "src/bindings.cpp"
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("include"),
                .headerSearchPath("src"),
                .headerSearchPath("drills"),
                .headerSearchPath("assistance"),
                .headerSearchPath("scoring"),
                .headerSearchPath("controller")
            ]
        ),
        .target(
            name: "CEarTrainerBridge",
            dependencies: ["EarTrainerEngine"],
            path: "Sources/CEarTrainerBridge",
            exclude: ["Engine"],
            publicHeadersPath: "include",
            linkerSettings: [
                .linkedLibrary("c++")
            ]
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
    ],
    cxxLanguageStandard: .cxx17
)
