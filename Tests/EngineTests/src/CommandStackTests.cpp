#include "TestFramework.hpp"
#include "Astral/Core/CommandStack.hpp"
#include "Astral/Scene/SceneCommands.hpp"
#include "Astral/Scene/Scene.hpp"

namespace Astral::Test {

class MockSimpleCommand : public ICommand {
public:
    explicit MockSimpleCommand(int& value, int delta, std::string name = "MockDelta")
        : m_Value(value), m_Delta(delta), m_Name(std::move(name)) {}

    void Execute() override { m_Value += m_Delta; }
    void Undo() override { m_Value -= m_Delta; }
    [[nodiscard]] std::string GetName() const override { return m_Name; }

private:
    int& m_Value;
    int m_Delta;
    std::string m_Name;
};

void RunCommandStackTests() {
    const std::string suite = "CommandStackSuite";

    // 1. Temel Undo / Redo Calismasi
    CommandStack stack(10);
    int testVal = 0;

    TEST_CHECK(suite, "InitiallyEmptyStack", !stack.CanUndo() && !stack.CanRedo());
    TEST_CHECK(suite, "InitialCountsZero", stack.GetUndoCount() == 0 && stack.GetRedoCount() == 0);

    stack.PushAndExecute(std::make_unique<MockSimpleCommand>(testVal, 5, "Add5"));
    TEST_CHECK(suite, "CommandExecuted", testVal == 5);
    TEST_CHECK(suite, "CanUndoAfterPush", stack.CanUndo() && !stack.CanRedo());
    TEST_CHECK(suite, "UndoNameMatches", stack.GetUndoName() == "Add5");

    stack.PushAndExecute(std::make_unique<MockSimpleCommand>(testVal, 10, "Add10"));
    TEST_CHECK(suite, "SecondCommandExecuted", testVal == 15);
    TEST_CHECK(suite, "UndoCountIsTwo", stack.GetUndoCount() == 2);

    // Undo testi
    stack.Undo();
    TEST_CHECK(suite, "FirstUndoRestoresValue", testVal == 5);
    TEST_CHECK(suite, "CanRedoAfterUndo", stack.CanRedo());
    TEST_CHECK(suite, "RedoNameMatches", stack.GetRedoName() == "Add10");

    // Redo testi
    stack.Redo();
    TEST_CHECK(suite, "RedoReappliesValue", testVal == 15);
    TEST_CHECK(suite, "CannotRedoAfterRedo", !stack.CanRedo());

    // Yeni komut gelince Redo stack temizlenmeli
    stack.Undo(); // testVal = 5
    TEST_CHECK(suite, "CanRedoTrueBeforeNewCommand", stack.CanRedo());
    stack.PushAndExecute(std::make_unique<MockSimpleCommand>(testVal, 2, "Add2")); // testVal = 7
    TEST_CHECK(suite, "RedoStackClearedOnNewPush", !stack.CanRedo());
    TEST_CHECK(suite, "NewPushValueCorrect", testVal == 7);

    // 2. Maksimum Kapasite Sinirlamasi (Max History Truncation)
    CommandStack boundedStack(3);
    int boundedVal = 0;
    boundedStack.PushAndExecute(std::make_unique<MockSimpleCommand>(boundedVal, 1));
    boundedStack.PushAndExecute(std::make_unique<MockSimpleCommand>(boundedVal, 1));
    boundedStack.PushAndExecute(std::make_unique<MockSimpleCommand>(boundedVal, 1));
    boundedStack.PushAndExecute(std::make_unique<MockSimpleCommand>(boundedVal, 1)); // 4. komut, ilki silinmeli
    TEST_CHECK(suite, "MaxHistoryRespected", boundedStack.GetUndoCount() == 3);

    // 3. Scene Komutlari: CreateEntityCommand & DeleteEntityCommand Entegrasyonu
    Scene scene("CommandTestScene");
    Entity selected;

    TransformComponent t1;
    t1.position = glm::vec3(1.0f, 2.0f, 3.0f);
    SDFComponent s1;
    s1.primitiveType = 0; // Sphere
    s1.blendFactor = 0.5f;

    // Create komutu yurutme
    stack.PushAndExecute(std::make_unique<CreateEntityCommand>(scene, "TestSphere", t1, s1, &selected));
    TEST_CHECK(suite, "EntityCreatedAndSelected", selected.IsValid());
    TEST_CHECK(suite, "EntityInSceneCountOne", scene.GetRegistry().GetAliveEntityCount() == 1);
    TEST_CHECK(suite, "EntityComponentPositionMatches",
               selected.GetComponent<TransformComponent>().position.x == 1.0f);

    // Create komutu geri alma (Undo)
    stack.Undo();
    TEST_CHECK(suite, "EntityDestroyedOnUndo", scene.GetRegistry().GetAliveEntityCount() == 0);
    TEST_CHECK(suite, "SelectedClearedOnUndo", !selected.IsValid());

    // Create komutu yeniden yapma (Redo)
    stack.Redo();
    TEST_CHECK(suite, "EntityRestoredOnRedo", scene.GetRegistry().GetAliveEntityCount() == 1);
    TEST_CHECK(suite, "SelectedReassignedOnRedo", selected.IsValid());

    // Delete komutu yurutme
    Entity entityToDelete = selected;
    stack.PushAndExecute(std::make_unique<DeleteEntityCommand>(scene, entityToDelete, &selected));
    TEST_CHECK(suite, "EntityDestroyedOnDeleteCommand", scene.GetRegistry().GetAliveEntityCount() == 0);
    TEST_CHECK(suite, "SelectedClearedOnDelete", !selected.IsValid());

    // Delete komutu geri alma (Undo) -> Sahneye bilesenleriyle geri gelmeli
    stack.Undo();
    TEST_CHECK(suite, "EntityRestoredOnDeleteUndo", scene.GetRegistry().GetAliveEntityCount() == 1);
    TEST_CHECK(suite, "SelectedRestoredOnDeleteUndo", selected.IsValid());
    TEST_CHECK(suite, "RestoredEntityHasTag", selected.HasComponent<TagComponent>());
    TEST_CHECK(suite, "RestoredEntityHasCorrectPosition",
               selected.GetComponent<TransformComponent>().position.y == 2.0f);
}

} // namespace Astral::Test
