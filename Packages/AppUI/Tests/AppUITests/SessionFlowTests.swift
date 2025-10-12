import XCTest
@testable import AppUI

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
            viewModel.start()
        }

        await MainActor.run {
            guard let first = viewModel.currentQuestion else {
                XCTFail("Missing first question")
                return
            }
            viewModel.submit(answer: first.choices.first?.id ?? "A", latencyMs: 120)
            viewModel.next()
            guard let second = viewModel.currentQuestion else {
                XCTFail("Missing second question")
                return
            }
            viewModel.submit(answer: second.choices.first?.id ?? "A", latencyMs: 95)
            viewModel.finish()
        }

        let route = await MainActor.run { viewModel.route }
        XCTAssertEqual(route, .results)

        let checkpointURL = try Paths.checkpointURL()
        XCTAssertFalse(FileManager.default.fileExists(atPath: checkpointURL.path))

        let profileURL = try Paths.profileURL(for: "tester")
        let profileData = try Data(contentsOf: profileURL)
        XCTAssertFalse(profileData.isEmpty)
    }

    func testCheckpointRestorationBringsSessionBackToGame() async throws {
        let bridge = MockBridge()
        let viewModel = await MainActor.run { SessionViewModel(engine: bridge, profileName: "checkpoint") }

        await MainActor.run {
            viewModel.bootstrap()
            viewModel.start()
            guard let question = viewModel.currentQuestion else { return }
            viewModel.submit(answer: question.choices.first?.id ?? "A", latencyMs: 60)
            viewModel.handleScenePhase(.background)
        }

        try await Task.sleep(nanoseconds: 300_000_000)

        let checkpointURL = try Paths.checkpointURL()
        XCTAssertTrue(FileManager.default.fileExists(atPath: checkpointURL.path))

        let newBridge = MockBridge()
        let restoredVM = await MainActor.run { SessionViewModel(engine: newBridge, profileName: "checkpoint") }
        await MainActor.run {
            restoredVM.bootstrap()
        }
        let route = await MainActor.run { restoredVM.route }
        XCTAssertEqual(route, .game)
        let questionId = await MainActor.run { restoredVM.currentQuestion?.id }
        XCTAssertEqual(questionId, "q2")
    }
}
