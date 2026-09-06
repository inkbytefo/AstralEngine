#include "Astral/Editor/SelectionOperations.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

namespace Astral { void RunSelectionInteractionTests(); }
using namespace Astral;
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool Near(const glm::mat4& a, const glm::mat4& b) {
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
        if (std::abs(a[c][r] - b[c][r]) > 0.001f) return false;
    return true;
}
Entity Make(Scene& scene, const char* name, glm::vec3 position = {}) {
    Entity e = scene.CreateEntity();
    e.AddComponent<TagComponent>(name);
    e.AddComponent<TransformComponent>().position = position;
    return e;
}
void SelectionTests() {
    Scene scene, other;
    auto a = Make(scene, "A"), b = Make(scene, "B"), c = Make(scene, "C");
    SelectionContext s;
    s.SelectEntity(a); s.SelectEntity(b, true); s.SelectEntity(c, true); s.SelectEntity(b, true);
    Check(s.Entities().size() == 3 && s.Primary() == b, "Additive uniqueness / primary");
    s.Toggle(b);
    Check(s.Entities().size() == 2 && s.Primary() == c, "Ctrl deselection / fallback");
    s.Toggle(b); s.SelectEntity(a);
    Check(s.Entities().size() == 1 && s.Primary() == a, "Normal selection replaces group");
    s.SelectEntity(b, true); scene.DestroyEntity(b); s.Reconcile();
    Check(s.Entities().size() == 1 && s.Primary() == a, "Deleted primary pruned");
    const auto reused = Make(scene, "Reused");
    Check(!s.IsEntitySelected(reused), "Generational handle must not revive selection");
    s.SelectEntity(Make(other, "Other"), true);
    Check(s.Entities().size() == 1, "Cross-scene selection isolated");
    s.ClearSelection(); Check(!s.Primary().IsValid() && s.Entities().empty(), "Clear");
}
void TransformTests() {
    Scene scene;
    auto a = Make(scene, "A", {1, 2, 3}), b = Make(scene, "B", {-2, 0, 1}), c = Make(scene, "C", {0, 4, 0});
    SelectionContext s; s.SelectEntity(a); s.SelectEntity(b, true); s.SelectEntity(c, true);
    for (const auto& delta : {
        glm::translate(glm::mat4(1), glm::vec3(4, -2, 1)),
        glm::rotate(glm::mat4(1), glm::radians(45.0f), glm::vec3(0, 1, 0)),
        glm::scale(glm::mat4(1), glm::vec3(2))}) {
        const auto wa = scene.GetWorldTransform(a), wb = scene.GetWorldTransform(b), wc = scene.GetWorldTransform(c);
        Check(ApplySelectionDelta(scene, s, delta), "Apply group delta");
        Check(Near(scene.GetWorldTransform(a), delta * wa) && Near(scene.GetWorldTransform(b), delta * wb) && Near(scene.GetWorldTransform(c), delta * wc), "Translate / rotate / scale group");
    }
    auto parent = Make(scene, "Parent", {4, 0, 2});
    parent.GetComponent<TransformComponent>().rotation = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 1, 0));
    parent.GetComponent<TransformComponent>().scale = {2, 3, 4};
    Check(scene.SetParent(a, parent), "Parent setup");
    s.SelectEntity(a); s.SelectEntity(b, true);
    const auto before = scene.GetWorldTransform(a);
    const glm::mat4 move = glm::translate(glm::mat4(1), glm::vec3(3, 5, 7));
    Check(ApplySelectionDelta(scene, s, move) && Near(scene.GetWorldTransform(a), move * before), "World delta under rotated scaled parent");
    s.SelectEntity(parent, true);
    const auto childBefore = scene.GetWorldTransform(a);
    const auto localBefore = GetLocalTransformMatrix(a.GetComponent<TransformComponent>());
    Check(ApplySelectionDelta(scene, s, move), "Selected parent and child delta");
    Check(Near(scene.GetWorldTransform(a), move * childBefore), "Child moves exactly once");
    Check(Near(GetLocalTransformMatrix(a.GetComponent<TransformComponent>()), localBefore), "Selected child local transform preserved");
    s.SelectEntity(a); parent.GetComponent<TransformComponent>().scale = {0, 0, 0};
    Check(!ApplySelectionDelta(scene, s, move), "Singular parent safely rejected");
}
void CommandTests() {
    Scene scene;
    auto parent = Make(scene, "Parent"), child = Make(scene, "Child"), outside = Make(scene, "Outside");
    Check(scene.SetParent(child, parent), "Hierarchy");
    child.AddComponent<HealthComponent>().hp = 42;
    child.AddComponent<VisibilityComponent>().isVisible = false;
    child.AddComponent<CameraComponent>().primary = 1;
    SelectionContext s; s.SelectEntity(child); s.SelectEntity(parent, true);
    CommandStack stack;
    EditSelection(scene, s, &stack, false);
    Check(scene.GetRegistry().GetAliveEntityCount() == 1 && outside.IsValid(), "Delete subtree once; unrelated entity survives");
    Check(stack.GetUndoCount() == 1 && s.Entities().empty(), "Group delete is one undo");
    stack.Undo();
    Check(s.Entities().size() == 2 && scene.GetRegistry().GetAliveEntityCount() == 3, "Restore group selection");
    auto restoredParent = s.Primary();
    Check(restoredParent.GetComponent<TagComponent>().tag == "Parent", "Primary restored");
    auto restoredChild = Entity(scene.GetChildren(restoredParent).at(0), &scene);
    Check(restoredChild.GetComponent<HealthComponent>().hp == 42 && !restoredChild.GetComponent<VisibilityComponent>().isVisible && restoredChild.GetComponent<CameraComponent>().primary == 1, "Components and hierarchy restored");
    stack.Redo(); stack.Undo();
    Check(s.Entities().size() == 2 && scene.GetRegistry().GetAliveEntityCount() == 3, "Delete redo cycle");
    EditSelection(scene, s, &stack, true);
    Check(s.Entities().size() == 2 && scene.GetRegistry().GetAliveEntityCount() == 5, "Duplicate parent + child only once");
    auto copyChild = Entity(scene.GetChildren(s.Primary()).at(0), &scene);
    Check(copyChild.GetComponent<CameraComponent>().primary == 0, "Duplicate camera does not steal view");
    stack.Undo(); Check(scene.GetRegistry().GetAliveEntityCount() == 3, "Undo duplicate");
    stack.Redo(); Check(scene.GetRegistry().GetAliveEntityCount() == 5, "Redo duplicate");
}
}
int main() {
    try { SelectionTests(); TransformTests(); CommandTests(); RunSelectionInteractionTests(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    std::cout << "PASS: selection, group transforms, hierarchy, delete/duplicate undo-redo\n";
}


