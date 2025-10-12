import XCTest
@testable import AppUI
import Bridge

final class SessionFlowTests: XCTestCase {
    private var tempDirectory: URL!

    override func setUpWithError() throws {
        tempDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        Paths.setOverrideRoot(tempDirectory)
    }

    override func tearDownWithError() throws {
        Paths.setOverrideRoot(nil)
        if let tempDirectory {
            try? FileManager.default.removeItem(at: tempDirectory)
        }
        tempDirectory = nil
    }

    func testSessionLifecyclePersistsProfileAndClearsCheckpoint() async throws {
        let bridge = MockBridge()
        let viewModel = await MainActor.run { SessionViewModel(engine: bridge, profileName: "tester") }

        await MainActor.run {
            viewModel.bootstrap()
            viewModel.spec.nQuestions = 2
            viewModel.start()
        }

        await MainActor.run {
            XCTAssertNotNil(viewModel.currentQuestion, "Expected first question")
            viewModel.submit(latencyMs: 120)
            XCTAssertNotNil(viewModel.currentQuestion, "Expected second question")
            viewModel.submit(latencyMs: 95)
        }

        let route = await MainActor.run { viewModel.route }
        XCTAssertEqual(route, .results)

        let checkpointURL = try Paths.checkpointURL()
        XCTAssertFalse(FileManager.default.fileExists(atPath: checkpointURL.path))

        let profileURL = try Paths.profileURL(for: "tester")
        let profileData = try Data(contentsOf: profileURL)
        XCTAssertFalse(profileData.isEmpty)
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        let snapshot = try decoder.decode(ProfileSnapshot.self, from: profileData)
        XCTAssertEqual(snapshot.name, "tester")
    }

    func testCheckpointFileNotCreatedWhenEngineDoesNotSupportIt() async throws {
        let bridge = MockBridge()
        let viewModel = await MainActor.run { SessionViewModel(engine: bridge, profileName: "checkpoint") }

        await MainActor.run {
            viewModel.bootstrap()
            viewModel.start()
            viewModel.submit(latencyMs: 60)
            viewModel.handleScenePhase(.background)
        }

        try await Task.sleep(nanoseconds: 300_000_000)

        let checkpointURL = try Paths.checkpointURL()
        XCTAssertFalse(FileManager.default.fileExists(atPath: checkpointURL.path), "Checkpoint should not be created by the mock bridge")
    }
}
