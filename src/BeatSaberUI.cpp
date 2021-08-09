#include <fstream>
#include "BeatSaberUI.hpp"
#include "InternalBeatSaberUI.hpp"

#include "CustomTypes/Components/ExternalComponents.hpp"
#include "CustomTypes/Components/Backgroundable.hpp"
#include "CustomTypes/Components/ScrollViewContent.hpp"
#include "CustomTypes/Components/MainThreadScheduler.hpp"
#include "CustomTypes/Components/FloatingScreen/FloatingScreen.hpp"
#include "CustomTypes/Components/FloatingScreen/FloatingScreenManager.hpp"

#include "GlobalNamespace/UIKeyboardManager.hpp"
#include "GlobalNamespace/BoolSettingsController.hpp"
#include "GlobalNamespace/FormattedFloatListSettingsValueController.hpp"
#include "GlobalNamespace/ReleaseInfoViewController.hpp"
#include "GlobalNamespace/ColorPickerButtonController.hpp"
#include "GlobalNamespace/HSVPanelController.hpp"

#include "UnityEngine/Canvas.hpp"
#include "UnityEngine/CanvasGroup.hpp"
#include "UnityEngine/AdditionalCanvasShaderChannels.hpp"
#include "UnityEngine/RenderMode.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/SpriteMeshType.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/TextureWrapMode.hpp"
#include "UnityEngine/ImageConversion.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/UI/ScrollRect.hpp"
#include "UnityEngine/UI/CanvasScaler.hpp"
#include "UnityEngine/Events/UnityAction_1.hpp"
#include "UnityEngine/Events/UnityAction.hpp"

#include "HMUI/Touchable.hpp"
#include "HMUI/HoverHintController.hpp"
#include "HMUI/TableView.hpp"
#include "HMUI/TextPageScrollView.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "HMUI/TextSegmentedControl.hpp"
#include "HMUI/InputFieldView_InputFieldChanged.hpp"
#include "HMUI/UIKeyboard.hpp"
#include "HMUI/CurvedCanvasSettings.hpp"
#include "HMUI/EventSystemListener.hpp"
#include "HMUI/DropdownWithTableView.hpp"
#include "HMUI/ButtonSpriteSwap.hpp"

#include "VRUIControls/VRGraphicRaycaster.hpp"
#include "Polyglot/LocalizedTextMeshProUGUI.hpp"

#include "System/Convert.hpp"
#include "System/Action_2.hpp"
#include "System/Action.hpp"

#include "Zenject/DiContainer.hpp"

#include "customlogger.hpp"

#define DEFAULT_BUTTONTEMPLATE "PracticeButton"

using namespace GlobalNamespace;
using namespace UnityEngine;
using namespace UnityEngine::UI;
using namespace UnityEngine::Events;
using namespace TMPro;
using namespace HMUI;
using namespace Polyglot;
using namespace VRUIControls;
using namespace Zenject;

#define MakeDelegate(DelegateType, varName) (il2cpp_utils::MakeDelegate<DelegateType>(classof(DelegateType), varName))

namespace QuestUI::BeatSaberUI {

    GameObject* beatSaberUIObject = nullptr;
    GameObject* dropdownListPrefab = nullptr;
    GameObject* modalPrefab = nullptr;
    
    void SetupPersistentObjects() {
        getLogger().info("SetupPersistentObjects");
        if(!beatSaberUIObject) {
            static auto name = il2cpp_utils::createcsstr("BeatSaberUIObject", il2cpp_utils::StringType::Manual);
            beatSaberUIObject = GameObject::New_ctor(name);
            GameObject::DontDestroyOnLoad(beatSaberUIObject);
            beatSaberUIObject->AddComponent<MainThreadScheduler*>();
        }
        if(!dropdownListPrefab) {
            GameObject* search = ArrayUtil::First(Resources::FindObjectsOfTypeAll<SimpleTextDropdown*>(), [](SimpleTextDropdown* x) { 
                    return to_utf8(csstrtostr(x->get_transform()->get_parent()->get_name())) == "NormalLevels"; 
                }
            )->get_transform()->get_parent()->get_gameObject();
            dropdownListPrefab = Object::Instantiate(search, beatSaberUIObject->get_transform(), false);
            static auto name = il2cpp_utils::createcsstr("QuestUIDropdownListPrefab", il2cpp_utils::StringType::Manual);
            dropdownListPrefab->set_name(name);
            dropdownListPrefab->SetActive(false);
        }
        if (!modalPrefab) {
            GameObject* search = ArrayUtil::First(Resources::FindObjectsOfTypeAll<ModalView*>(), [](ModalView* x) { 
                    return to_utf8(csstrtostr(x->get_transform()->get_name())) == "DropdownTableView";
                }
            )->get_gameObject();
            modalPrefab = Object::Instantiate(search, beatSaberUIObject->get_transform(), false);
            
            modalPrefab->GetComponent<ModalView*>()->presentPanelAnimations = search->GetComponent<ModalView*>()->presentPanelAnimations;
            modalPrefab->GetComponent<ModalView*>()->dismissPanelAnimation = search->GetComponent<ModalView*>()->dismissPanelAnimation;

            static auto name = il2cpp_utils::createcsstr("QuestUIModalPrefab", il2cpp_utils::StringType::Manual);
            modalPrefab->set_name(name);
            modalPrefab->SetActive(false);
        }
    }

    #define CacheNotFoundWarningLog(type) getLogger().warning("Can't find '" #type "'! (This shouldn't happen and can cause unexpected behaviour)")

    MainFlowCoordinator* mainFlowCoordinator = nullptr;
    MainFlowCoordinator* GetMainFlowCoordinator() {
        if(!mainFlowCoordinator)
            mainFlowCoordinator = Object::FindObjectOfType<MainFlowCoordinator*>();
        if(!mainFlowCoordinator)
            CacheNotFoundWarningLog(MainFlowCoordinator);
        return mainFlowCoordinator;
    }

    TMP_FontAsset* mainTextFont = nullptr;
    TMP_FontAsset* GetMainTextFont() {
        if(!mainTextFont)
            mainTextFont = ArrayUtil::First(Resources::FindObjectsOfTypeAll<TMP_FontAsset*>(), [](TMP_FontAsset* x) { return to_utf8(csstrtostr(x->get_name())) == "Teko-Medium SDF"; });
        if(!mainTextFont)
            CacheNotFoundWarningLog(MainTextFont);
        return mainTextFont;
    }

    Material* mainUIFontMaterial = nullptr;
    Material* GetMainUIFontMaterial()
    {
        if(!mainUIFontMaterial)
            mainUIFontMaterial = ArrayUtil::First(Resources::FindObjectsOfTypeAll<Material*>(), [](Material* x) { return to_utf8(csstrtostr(x->get_name())) == "Teko-Medium SDF Curved Softer"; });
        if(!mainUIFontMaterial)
            CacheNotFoundWarningLog(MainUIFontMaterial);
        return mainUIFontMaterial;
    }

    Sprite* editIcon = nullptr;
    Sprite* GetEditIcon() {
        if(!editIcon)
            editIcon = ArrayUtil::First(Resources::FindObjectsOfTypeAll<Image*>(), [](Image* x) { return x->get_sprite() && to_utf8(csstrtostr(x->get_sprite()->get_name())) == "EditIcon"; })->get_sprite();
        if(!editIcon)
            CacheNotFoundWarningLog(EditIcon);
        return editIcon;
    }

    PhysicsRaycasterWithCache* physicsRaycaster = nullptr;
    PhysicsRaycasterWithCache* GetPhysicsRaycasterWithCache()
    {
        if(!physicsRaycaster)
            physicsRaycaster = ArrayUtil::First(Resources::FindObjectsOfTypeAll<MainMenuViewController*>())->GetComponent<VRGraphicRaycaster*>()->physicsRaycaster;
        if(!physicsRaycaster)
            CacheNotFoundWarningLog(PhysicsRaycasterWithCache);
        return physicsRaycaster;
    }

    DiContainer* diContainer = nullptr;
    DiContainer* GetDiContainer()
    {
        if(!diContainer)
            diContainer = ArrayUtil::First(Resources::FindObjectsOfTypeAll<TextSegmentedControl*>(), [](TextSegmentedControl* x) { return to_utf8(csstrtostr(x->get_transform()->get_parent()->get_name())) == "PlayerStatisticsViewController" && x->container; })->container;
        if(!diContainer)
            CacheNotFoundWarningLog(DiContainer);
        return diContainer;
    }

    void ClearCache() {
        mainFlowCoordinator = nullptr;
        mainTextFont = nullptr;
        mainUIFontMaterial = nullptr;
        editIcon = nullptr;
        physicsRaycaster = nullptr;
        diContainer = nullptr;
    }

    GameObject* CreateCanvas() {
        static auto name = il2cpp_utils::createcsstr("QuestUICanvas", il2cpp_utils::StringType::Manual);
        GameObject* go = GameObject::New_ctor(name);
        go->set_layer(5);
        Canvas* cv = go->AddComponent<Canvas*>();
        cv->set_additionalShaderChannels(AdditionalCanvasShaderChannels::TexCoord1 | AdditionalCanvasShaderChannels::TexCoord2);
        cv->set_sortingOrder(4);
        
        CanvasScaler* scaler = go->AddComponent<CanvasScaler*>();
        scaler->set_scaleFactor(1.0f);
        scaler->set_dynamicPixelsPerUnit(3.44f);
        scaler->set_referencePixelsPerUnit(10.0f);

        auto* physicsRaycaster = GetPhysicsRaycasterWithCache();
        if(physicsRaycaster)
            go->AddComponent<VRGraphicRaycaster*>()->physicsRaycaster = physicsRaycaster;

        RectTransform* rectTransform = go->GetComponent<RectTransform*>();
        float scale = 1.5f*0.02f; //Wrapper->ScreenSystem: 1.5 Wrapper->ScreenSystem->ScreenContainer: 0.02
        rectTransform->set_localScale(UnityEngine::Vector3(scale, scale, scale));
        return go;
    }

    ViewController* CreateViewController(System::Type* type) {
        static auto name = il2cpp_utils::createcsstr("QuestUIViewController", il2cpp_utils::StringType::Manual);
        GameObject* go = GameObject::New_ctor(name);

        Canvas* cv = go->AddComponent<Canvas*>();
        Canvas* cvCopy = ArrayUtil::First(Resources::FindObjectsOfTypeAll<Canvas*>(), [](Canvas* x) { return to_utf8(csstrtostr(x->get_name())) == "DropdownTableView";});
        cv->set_additionalShaderChannels(cvCopy->get_additionalShaderChannels());
        cv->set_overrideSorting(cvCopy->get_overrideSorting());
        cv->set_pixelPerfect(cvCopy->get_pixelPerfect());
        cv->set_referencePixelsPerUnit(cvCopy->get_referencePixelsPerUnit());
        cv->set_renderMode(cvCopy->get_renderMode());
        cv->set_scaleFactor(cvCopy->get_scaleFactor());
        cv->set_sortingLayerID(cvCopy->get_sortingLayerID());
        cv->set_sortingOrder(cvCopy->get_sortingOrder());
        cv->set_worldCamera(cvCopy->get_worldCamera());

        auto* physicsRaycaster = GetPhysicsRaycasterWithCache();
        if(physicsRaycaster)
            go->AddComponent<VRGraphicRaycaster*>()->physicsRaycaster = physicsRaycaster;
        go->AddComponent<CanvasGroup*>();
        ViewController* vc = reinterpret_cast<ViewController*>(go->AddComponent(type));

        RectTransform* rectTransform = go->GetComponent<RectTransform*>();
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.0f, 0.0f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(1.0f, 1.0f));
        rectTransform->set_sizeDelta(UnityEngine::Vector2(0.0f, 0.0f));
        rectTransform->set_anchoredPosition(UnityEngine::Vector2(0.0f, 0.0f));
        go->SetActive(false);
        return vc;
    }
    
    FlowCoordinator* CreateFlowCoordinator(System::Type* type) {
        static auto name = il2cpp_utils::createcsstr("QuestUIFlowCoordinator", il2cpp_utils::StringType::Manual);
        FlowCoordinator* flowCoordinator = reinterpret_cast<FlowCoordinator*>(GameObject::New_ctor(name)->AddComponent(type));
        flowCoordinator->baseInputModule = GetMainFlowCoordinator()->baseInputModule;
        return flowCoordinator;
    }

    TextMeshProUGUI* CreateText(Transform* parent, std::string text, UnityEngine::Vector2 anchoredPosition) {
        return CreateText(parent, text, true, anchoredPosition);
    }

    TextMeshProUGUI* CreateText(Transform* parent, std::string text, UnityEngine::Vector2 anchoredPosition, UnityEngine::Vector2 sizeDelta) {
        return CreateText(parent, text, true, anchoredPosition, sizeDelta);
    }

    TextMeshProUGUI* CreateText(Transform* parent, std::string text, bool italic) {
        return CreateText(parent, text, italic, UnityEngine::Vector2(0.0f, 0.0f), UnityEngine::Vector2(60.0f, 10.0f));
    }

    TextMeshProUGUI* CreateText(Transform* parent, std::string text, bool italic, UnityEngine::Vector2 anchoredPosition) {
        return CreateText(parent, text, italic, anchoredPosition, UnityEngine::Vector2(60.0f, 10.0f));
    }

    TextMeshProUGUI* CreateText(Transform* parent, std::string text, bool italic, UnityEngine::Vector2 anchoredPosition, UnityEngine::Vector2 sizeDelta) {
        static auto name = il2cpp_utils::createcsstr("QuestUIText", il2cpp_utils::StringType::Manual);
        GameObject* gameObj = GameObject::New_ctor(name);
        gameObj->SetActive(false);

        CurvedTextMeshPro* textMesh = gameObj->AddComponent<CurvedTextMeshPro*>();
        RectTransform* rectTransform = textMesh->get_rectTransform();
        rectTransform->SetParent(parent, false);
        textMesh->set_font(GetMainTextFont());
        textMesh->set_fontSharedMaterial(GetMainUIFontMaterial());
        textMesh->set_text(il2cpp_utils::createcsstr(italic ? ("<i>" + text + "</i>") : text));
        textMesh->set_fontSize(4.0f);
        textMesh->set_color(UnityEngine::Color::get_white());
        textMesh->set_richText(true);
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.5f, 0.5f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(0.5f, 0.5f));
        rectTransform->set_anchoredPosition(anchoredPosition);
        rectTransform->set_sizeDelta(sizeDelta);
        
        gameObj->AddComponent<LayoutElement*>();

        gameObj->SetActive(true);
        return textMesh;
    }

    void SetButtonText(Button* button, std::string text) {
        LocalizedTextMeshProUGUI* localizer = button->GetComponentInChildren<LocalizedTextMeshProUGUI*>();
        if (localizer)
            Object::Destroy(localizer);
        TextMeshProUGUI* textMesh = button->GetComponentInChildren<TextMeshProUGUI*>();
        if(textMesh)
            textMesh->set_text(il2cpp_utils::createcsstr(text));
    }

    void SetButtonTextSize(Button* button, float fontSize) {
        TextMeshProUGUI* textMesh = button->GetComponentInChildren<TextMeshProUGUI*>();
        if(textMesh)
            textMesh->set_fontSize(fontSize);
    }

    void ToggleButtonWordWrapping(Button* button, bool enableWordWrapping) {
        TextMeshProUGUI* textMesh = button->GetComponentInChildren<TextMeshProUGUI*>();
        if(textMesh)
            textMesh->set_enableWordWrapping(enableWordWrapping);
    }

    void SetButtonIcon(Button* button, Sprite* icon) {
        if(!icon) return;
        auto* array = button->GetComponentsInChildren<Image*>();
        if(array->Length() > 1)
            ArrayUtil::First(array, [](Image* x) { return to_utf8(csstrtostr(x->get_name())) == "Icon";})->set_sprite(icon);
    }

    void SetButtonBackground(Button* button, Sprite* background) {
        auto* array = button->GetComponentsInChildren<Image*>();
        if(array->Length() > 0)
            array->values[0]->set_sprite(background);
    }

    void SetButtonSprites(UnityEngine::UI::Button* button, UnityEngine::Sprite* inactive, UnityEngine::Sprite* active) {
        // make sure the textures are set to clamp
        inactive->get_texture()->set_wrapMode(TextureWrapMode::Clamp);
        active->get_texture()->set_wrapMode(TextureWrapMode::Clamp);

        ButtonSpriteSwap* spriteSwap = button->GetComponent<ButtonSpriteSwap*>();

        // setting the sprites
        spriteSwap->highlightStateSprite = active;
        spriteSwap->pressedStateSprite = active;

        spriteSwap->disabledStateSprite = inactive;
        spriteSwap->normalStateSprite = inactive;
    }

    Button* CreateUIButton(Transform* parent, std::string buttonText, std::string buttonTemplate, std::function<void()> onClick) {
        Button* button = Object::Instantiate(ArrayUtil::Last(Resources::FindObjectsOfTypeAll<Button*>(), [&buttonTemplate](Button* x) { return to_utf8(csstrtostr(x->get_name())) == buttonTemplate; }), parent, false);
        button->set_onClick(Button::ButtonClickedEvent::New_ctor());
        static auto name = il2cpp_utils::createcsstr("QuestUIButton", il2cpp_utils::StringType::Manual);
        button->set_name(name);
        if(onClick)
            button->get_onClick()->AddListener(MakeDelegate(UnityAction*, onClick));

        LocalizedTextMeshProUGUI* localizer = button->GetComponentInChildren<LocalizedTextMeshProUGUI*>();
        if (localizer != nullptr)
            GameObject::Destroy(localizer);
        ExternalComponents* externalComponents = button->get_gameObject()->AddComponent<ExternalComponents*>();

        TextMeshProUGUI* textMesh = button->GetComponentInChildren<TextMeshProUGUI*>();
        textMesh->set_richText(true);
        textMesh->set_alignment(TextAlignmentOptions::Center);
        externalComponents->Add(textMesh);

        RectTransform* rectTransform = (RectTransform*)button->get_transform();
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.5f, 0.5f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(0.5f, 0.5f));

        SetButtonText(button, buttonText);

        HorizontalLayoutGroup* horiztonalLayoutGroup = button->GetComponentInChildren<HorizontalLayoutGroup*>();
        if (horiztonalLayoutGroup != nullptr)
            externalComponents->Add(horiztonalLayoutGroup);
        
        // if the original button was for some reason not interactable, now it will be
        button->set_interactable(true);
        return button;
    }

    Button* CreateUIButton(Transform* parent, std::string buttonText, std::string buttonTemplate, UnityEngine::Vector2 anchoredPosition, std::function<void()> onClick) {
        Button* button = CreateUIButton(parent, buttonText, buttonTemplate, onClick);
        button->GetComponent<RectTransform*>()->set_anchoredPosition(anchoredPosition);
        return button;
    }

    Button* CreateUIButton(Transform* parent, std::string buttonText, std::string buttonTemplate, UnityEngine::Vector2 anchoredPosition, UnityEngine::Vector2 sizeDelta, std::function<void()> onClick) {
        Button* button = CreateUIButton(parent, buttonText, buttonTemplate, anchoredPosition, onClick);
        button->GetComponent<RectTransform*>()->set_sizeDelta(sizeDelta);
        LayoutElement* layoutElement = button->GetComponent<LayoutElement*>();
        if(!layoutElement)
            layoutElement = button->get_gameObject()->AddComponent<LayoutElement*>();
        layoutElement->set_minWidth(sizeDelta.x);
        layoutElement->set_minHeight(sizeDelta.y);
        layoutElement->set_preferredWidth(sizeDelta.x);
        layoutElement->set_preferredHeight(sizeDelta.y);
        layoutElement->set_flexibleWidth(sizeDelta.x);
        layoutElement->set_flexibleHeight(sizeDelta.y);
        return button;
    }

    Button* CreateUIButton(Transform* parent, std::string buttonText, std::function<void()> onClick) {
        return CreateUIButton(parent, buttonText, DEFAULT_BUTTONTEMPLATE, onClick);
    }

    Button* CreateUIButton(Transform* parent, std::string buttonText, UnityEngine::Vector2 anchoredPosition, std::function<void()> onClick) {
        return CreateUIButton(parent, buttonText, DEFAULT_BUTTONTEMPLATE, anchoredPosition, onClick);
    }

    Button* CreateUIButton(Transform* parent, std::string buttonText, UnityEngine::Vector2 anchoredPosition, UnityEngine::Vector2 sizeDelta, std::function<void()> onClick) {
        return CreateUIButton(parent, buttonText, DEFAULT_BUTTONTEMPLATE, anchoredPosition, sizeDelta, onClick);
    }

    Image* CreateImage(Transform* parent, Sprite* sprite, UnityEngine::Vector2 anchoredPosition, UnityEngine::Vector2 sizeDelta) {
        static auto name = il2cpp_utils::createcsstr("QuestUIImage", il2cpp_utils::StringType::Manual);
        GameObject* gameObj = GameObject::New_ctor(name);
        Image* image = gameObj->AddComponent<Image*>();
        image->get_transform()->SetParent(parent, false);
        image->set_sprite(sprite);
        RectTransform* rectTransform = (RectTransform*)image->get_transform();
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.5f, 0.5f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(0.5f, 0.5f));
        rectTransform->set_anchoredPosition(anchoredPosition);
        rectTransform->set_sizeDelta(sizeDelta);
        
        gameObj->AddComponent<LayoutElement*>();
        return image;
    }

    Sprite* Base64ToSprite(std::string& base64, int width, int height)
    {
        return Base64ToSprite(base64);
    }

    Sprite* Base64ToSprite(std::string& base64)
    {
        Array<uint8_t>* bytes = System::Convert::FromBase64String(il2cpp_utils::createcsstr(base64));
        return ArrayToSprite(bytes);
    }

    Sprite* FileToSprite(std::string& filePath, int width, int height)
    {
        return FileToSprite(filePath);
    }

    Sprite* FileToSprite(std::string& filePath)
    {
        std::ifstream instream(filePath, std::ios::in | std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(instream)), std::istreambuf_iterator<char>());
        Array<uint8_t>* bytes = il2cpp_utils::vectorToArray(data);
        return ArrayToSprite(bytes);
    }

    Sprite* ArrayToSprite(Array<uint8_t>* bytes)
    {
        Texture2D* texture = Texture2D::New_ctor(0, 0, TextureFormat::RGBA32, false, false);
        if (ImageConversion::LoadImage(texture, bytes, false)) {
            texture->set_wrapMode(TextureWrapMode::Clamp);
            return Sprite::Create(texture, UnityEngine::Rect(0.0f, 0.0f, (float)texture->get_width(), (float)texture->get_height()), UnityEngine::Vector2(0.5f,0.5f), 1024.0f, 1u, SpriteMeshType::FullRect, UnityEngine::Vector4(0.0f, 0.0f, 0.0f, 0.0f), false);
        }
        return nullptr;
    }

    GridLayoutGroup* CreateGridLayoutGroup(Transform* parent) {
        static auto name = il2cpp_utils::createcsstr("QuestUIGridLayoutGroup", il2cpp_utils::StringType::Manual);
        GameObject* gameObject = GameObject::New_ctor(name);
        GridLayoutGroup* layout = gameObject->AddComponent<GridLayoutGroup*>();
        gameObject->AddComponent<ContentSizeFitter*>();
        gameObject->AddComponent<Backgroundable*>();

        RectTransform* rectTransform = gameObject->GetComponent<RectTransform*>();
        rectTransform->SetParent(parent, false);
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.0f, 0.0f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(1.0f, 1.0f));
        rectTransform->set_sizeDelta(UnityEngine::Vector2(0.0f, 0.0f));

        gameObject->AddComponent<LayoutElement*>();
        return layout;
    }
    
    HorizontalLayoutGroup* CreateHorizontalLayoutGroup(Transform* parent) {
        static auto name = il2cpp_utils::createcsstr("QuestUIHorizontalLayoutGroup", il2cpp_utils::StringType::Manual);
        GameObject* gameObject = GameObject::New_ctor(name);
        HorizontalLayoutGroup* layout = gameObject->AddComponent<HorizontalLayoutGroup*>();
        gameObject->AddComponent<Backgroundable*>();

        ContentSizeFitter* contentSizeFitter = gameObject->AddComponent<ContentSizeFitter*>();
        contentSizeFitter->set_verticalFit(ContentSizeFitter::FitMode::PreferredSize);

        RectTransform* rectTransform = gameObject->GetComponent<RectTransform*>();
        rectTransform->SetParent(parent, false);
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.0f, 0.0f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(1.0f, 1.0f));
        rectTransform->set_sizeDelta(UnityEngine::Vector2(0.0f, 0.0f));
        
        gameObject->AddComponent<LayoutElement*>();
        return layout;
    }
    
    VerticalLayoutGroup* CreateVerticalLayoutGroup(Transform* parent) {
        static auto name = il2cpp_utils::createcsstr("QuestUIVerticalLayoutGroup", il2cpp_utils::StringType::Manual);
        GameObject* gameObject = GameObject::New_ctor(name);
        VerticalLayoutGroup* layout = gameObject->AddComponent<VerticalLayoutGroup*>();
        gameObject->AddComponent<Backgroundable*>();

        ContentSizeFitter* contentSizeFitter = gameObject->AddComponent<ContentSizeFitter*>();
        contentSizeFitter->set_horizontalFit(ContentSizeFitter::FitMode::PreferredSize);

        RectTransform* rectTransform = gameObject->GetComponent<RectTransform*>();
        rectTransform->SetParent(parent, false);
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.0f, 0.0f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(1.0f, 1.0f));
        rectTransform->set_sizeDelta(UnityEngine::Vector2(0.0f, 0.0f));
        
        gameObject->AddComponent<LayoutElement*>();
        return layout;
    }
    Toggle* CreateToggle(Transform* parent, std::string text, std::function<void(bool)> onValueChange)
    {
        return CreateToggle(parent, text, false, onValueChange);
    }

    Toggle* CreateToggle(Transform* parent, std::string text, bool currentValue, std::function<void(bool)> onValueChange)
    {
        return CreateToggle(parent, text, currentValue, UnityEngine::Vector2(0.0f, 0.0f), onValueChange);
    }

    Toggle* CreateToggle(Transform* parent, std::string text, UnityEngine::Vector2 anchoredPosition, std::function<void(bool)> onValueChange)
    {
        return CreateToggle(parent, text, false, anchoredPosition, onValueChange);
    }
    
    Toggle* CreateToggle(Transform* parent, std::string text, bool currentValue, UnityEngine::Vector2 anchoredPosition, std::function<void(bool)> onValueChange)
    {
        GameObject* gameObject = Object::Instantiate(ArrayUtil::First(ArrayUtil::Select<GameObject*>(Resources::FindObjectsOfTypeAll<Toggle*>(), [](Toggle* x){ return x->get_transform()->get_parent()->get_gameObject(); }), [](GameObject* x){ return to_utf8(csstrtostr(x->get_name())) == "Fullscreen";}), parent, false);
        GameObject* nameText = gameObject->get_transform()->Find(il2cpp_utils::createcsstr("NameText"))->get_gameObject();
        Object::Destroy(gameObject->GetComponent<BoolSettingsController*>());

        static auto name = il2cpp_utils::createcsstr("QuestUICheckboxSetting", il2cpp_utils::StringType::Manual);
        gameObject->set_name(name);

        gameObject->SetActive(false);

        Object::Destroy(nameText->GetComponent<LocalizedTextMeshProUGUI*>());
        
        Toggle* toggle = gameObject->GetComponentInChildren<Toggle*>();
        toggle->set_interactable(true);
        toggle->set_isOn(currentValue);
        toggle->onValueChanged = Toggle::ToggleEvent::New_ctor();
        if(onValueChange)
            toggle->onValueChanged->AddListener(MakeDelegate(UnityAction_1<bool>*, onValueChange));
        TextMeshProUGUI* textMesh = nameText->GetComponent<TextMeshProUGUI*>();
        textMesh->SetText(il2cpp_utils::createcsstr(text));
        textMesh->set_richText(true);
        RectTransform* rectTransform = gameObject->GetComponent<RectTransform*>();
        rectTransform->set_anchoredPosition(anchoredPosition);

        LayoutElement* layout = gameObject->GetComponent<LayoutElement*>();
        layout->set_preferredWidth(90.0f);
        gameObject->SetActive(true);
        return toggle;
    }

    /*GameObject* CreateLoadingIndicator(Transform* parent, UnityEngine::Vector2 anchoredPosition)
    {
        GameObject* loadingIndicator = GameObject::Instantiate(ArrayUtil::First(Resources::FindObjectsOfTypeAll<GameObject*>(), [](GameObject* x){ return to_utf8(csstrtostr(x->get_name())) == "LoadingIndicator"; }), parent, false);
        static auto name = il2cpp_utils::createcsstr("QuestUILoadingIndicator", il2cpp_utils::StringType::Manual);
        loadingIndicator->set_name(name);

        loadingIndicator->AddComponent<LayoutElement*>();

        RectTransform* rectTransform = loadingIndicator->GetComponent<RectTransform*>();
        rectTransform->set_anchoredPosition(anchoredPosition);

        return loadingIndicator;
    }*/

    HoverHint* AddHoverHint(GameObject* gameObject, std::string text){
        HoverHint* hoverHint = gameObject->AddComponent<HoverHint*>();
        hoverHint->set_text(il2cpp_utils::createcsstr(text));
        hoverHint->hoverHintController = ArrayUtil::First(Resources::FindObjectsOfTypeAll<HoverHintController*>());
        return hoverHint;
    }

    IncrementSetting* CreateIncrementSetting(Transform* parent, std::string text, int decimals, float increment, float currentValue, std::function<void(float)> onValueChange) {
        return CreateIncrementSetting(parent, text, decimals, increment, currentValue, UnityEngine::Vector2(0.0f, 0.0f), onValueChange);
    }

    IncrementSetting* CreateIncrementSetting(Transform* parent, std::string text, int decimals, float increment, float currentValue, float minValue, float maxValue, std::function<void(float)> onValueChange) {
        return CreateIncrementSetting(parent, text, decimals, increment, currentValue, minValue, maxValue, UnityEngine::Vector2(0.0f, 0.0f), onValueChange);
    }

    IncrementSetting* CreateIncrementSetting(Transform* parent, std::string text, int decimals, float increment, float currentValue, UnityEngine::Vector2 anchoredPosition, std::function<void(float)> onValueChange) {
        return CreateIncrementSetting(parent, text, decimals, increment, currentValue, false, false, 0.0f, 0.0f, anchoredPosition, onValueChange);
    }

    IncrementSetting* CreateIncrementSetting(Transform* parent, std::string text, int decimals, float increment, float currentValue, float minValue, float maxValue, UnityEngine::Vector2 anchoredPosition, std::function<void(float)> onValueChange) {
        return CreateIncrementSetting(parent, text, decimals, increment, currentValue, true, true, minValue, maxValue, anchoredPosition, onValueChange);
    }

    IncrementSetting* CreateIncrementSetting(Transform* parent, std::string text, int decimals, float increment, float currentValue, bool hasMin, bool hasMax, float minValue, float maxValue, UnityEngine::Vector2 anchoredPosition, std::function<void(float)> onValueChange) {
        FormattedFloatListSettingsValueController* baseSetting = Object::Instantiate(ArrayUtil::First(Resources::FindObjectsOfTypeAll<FormattedFloatListSettingsValueController*>(), [](FormattedFloatListSettingsValueController* x){ return to_utf8(csstrtostr(x->get_name())) == "VRRenderingScale"; }), parent, false);
        static auto name = il2cpp_utils::createcsstr("QuestUIIncDecSetting", il2cpp_utils::StringType::Manual);
        baseSetting->set_name(name);
        
        GameObject* gameObject = baseSetting->get_gameObject();
        Object::Destroy(baseSetting);
        gameObject->SetActive(false);

        IncrementSetting* setting = gameObject->AddComponent<IncrementSetting*>();
        setting->Decimals = decimals;
        setting->Increment = increment;
        setting->HasMin = hasMin;
        setting->HasMax = hasMax;
        setting->MinValue = minValue;
        setting->MaxValue = maxValue;
        setting->CurrentValue = currentValue;
        setting->OnValueChange = onValueChange;
        Transform* child = gameObject->get_transform()->GetChild(1);
        setting->Text = child->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
        setting->Text->SetText(setting->GetRoundedString());
        setting->Text->set_richText(true);
        Button* decButton = ArrayUtil::First(child->GetComponentsInChildren<Button*>());
        Button* incButton = ArrayUtil::Last(child->GetComponentsInChildren<Button*>());
        decButton->set_interactable(true);
        incButton->set_interactable(true);
        decButton->get_onClick()->AddListener(MakeDelegate(UnityAction*, (std::function<void()>)[setting](){ setting->DecButtonPressed(); }));
        incButton->get_onClick()->AddListener(MakeDelegate(UnityAction*, (std::function<void()>)[setting](){ setting->IncButtonPressed(); }));
        
        child->GetComponent<RectTransform*>()->set_sizeDelta(UnityEngine::Vector2(40, 0));
        TextMeshProUGUI* textMesh = gameObject->GetComponentInChildren<TextMeshProUGUI*>();
        textMesh->SetText(il2cpp_utils::createcsstr(text));
        textMesh->set_richText(true);
        gameObject->AddComponent<ExternalComponents*>()->Add(textMesh);

        Object::Destroy(textMesh->GetComponent<LocalizedTextMeshProUGUI*>());

        LayoutElement* layoutElement = gameObject->AddComponent<LayoutElement*>();
        layoutElement->set_preferredWidth(90.0f);

        RectTransform* rectTransform = gameObject->GetComponent<RectTransform*>();
        rectTransform->set_anchoredPosition(anchoredPosition);

        gameObject->SetActive(true);    

        return setting;
    }

    GameObject* CreateScrollView(Transform* parent) {
        TextPageScrollView* textScrollView = Object::Instantiate(ArrayUtil::First(Resources::FindObjectsOfTypeAll<ReleaseInfoViewController*>())->textPageScrollView, parent);
        static auto textScrollViewName = il2cpp_utils::createcsstr("QuestUIScrollView", il2cpp_utils::StringType::Manual);
        textScrollView->set_name(textScrollViewName);
        Button* pageUpButton = textScrollView->pageUpButton;
        Button* pageDownButton = textScrollView->pageDownButton;
        VerticalScrollIndicator* verticalScrollIndicator = textScrollView->verticalScrollIndicator; 
        RectTransform* viewport = textScrollView->viewport;

        auto* physicsRaycaster = GetPhysicsRaycasterWithCache();
        if(physicsRaycaster)
            viewport->get_gameObject()->AddComponent<VRGraphicRaycaster*>()->physicsRaycaster = physicsRaycaster;
        
        GameObject::Destroy(textScrollView->text->get_gameObject());
        GameObject* gameObject = textScrollView->get_gameObject();
        GameObject::Destroy(textScrollView);
        gameObject->SetActive(false);

        ScrollView* scrollView = gameObject->AddComponent<ScrollView*>();
        scrollView->pageUpButton = pageUpButton;
        scrollView->pageDownButton = pageDownButton;
        scrollView->verticalScrollIndicator = verticalScrollIndicator;
        scrollView->viewport = viewport;

        viewport->set_anchorMin(UnityEngine::Vector2(0.0f, 0.0f));
        viewport->set_anchorMax(UnityEngine::Vector2(1.0f, 1.0f));

        static auto parentObjName = il2cpp_utils::createcsstr("QuestUIScrollViewContent", il2cpp_utils::StringType::Manual);
        GameObject* parentObj = GameObject::New_ctor(parentObjName);
        parentObj->get_transform()->SetParent(viewport, false);

        ContentSizeFitter* contentSizeFitter = parentObj->AddComponent<ContentSizeFitter*>();
        contentSizeFitter->set_horizontalFit(ContentSizeFitter::FitMode::PreferredSize);
        contentSizeFitter->set_verticalFit(ContentSizeFitter::FitMode::PreferredSize);

        VerticalLayoutGroup* verticalLayout = parentObj->AddComponent<VerticalLayoutGroup*>();
        verticalLayout->set_childForceExpandHeight(false);
        verticalLayout->set_childForceExpandWidth(false);
        verticalLayout->set_childControlHeight(true);
        verticalLayout->set_childControlWidth(true);
        verticalLayout->set_childAlignment(TextAnchor::UpperCenter);

        RectTransform* rectTransform = parentObj->GetComponent<RectTransform*>();
        rectTransform->set_anchorMin(UnityEngine::Vector2(0.0f, 1.0f));
        rectTransform->set_anchorMax(UnityEngine::Vector2(1.0f, 1.0f));
        rectTransform->set_sizeDelta(UnityEngine::Vector2(0.0f, 0.0f));
        rectTransform->set_pivot(UnityEngine::Vector2(0.5f, 1.0f));
        
        parentObj->AddComponent<ScrollViewContent*>()->scrollView = scrollView;

        static auto childName = il2cpp_utils::createcsstr("QuestUIScrollViewContentContainer", il2cpp_utils::StringType::Manual);
        GameObject* child = GameObject::New_ctor(childName);
        child->get_transform()->SetParent(rectTransform, false);

        VerticalLayoutGroup* layoutGroup = child->AddComponent<VerticalLayoutGroup*>();
        layoutGroup->set_childControlHeight(false);
        layoutGroup->set_childForceExpandHeight(false);
        layoutGroup->set_childAlignment(TextAnchor::LowerCenter);
        layoutGroup->set_spacing(0.5f);

        ExternalComponents* externalComponents = child->AddComponent<ExternalComponents*>();
        externalComponents->Add(scrollView);
        externalComponents->Add(scrollView->get_transform());
        externalComponents->Add(gameObject->AddComponent<LayoutElement*>());

        child->GetComponent<RectTransform*>()->set_sizeDelta(UnityEngine::Vector2(0.0f, -1.0f));

        scrollView->contentRectTransform = rectTransform;

        gameObject->SetActive(true);
        return child;
    }

    GameObject* CreateScrollableSettingsContainer(Transform* parent) {
        GameObject* content = CreateScrollView(parent);
        ExternalComponents* externalComponents = content->GetComponent<ExternalComponents*>();
        RectTransform* scrollTransform = externalComponents->Get<RectTransform*>();
        scrollTransform->set_anchoredPosition(UnityEngine::Vector2(0.0f, 0.0f));
        scrollTransform->set_sizeDelta(UnityEngine::Vector2(-54.0f, 0.0f));
        static auto name = il2cpp_utils::createcsstr("QuestUIScrollableSettingsContainer", il2cpp_utils::StringType::Manual);
        scrollTransform->get_gameObject()->set_name(name);
        return content;
    }

    InputFieldView* CreateStringSetting(Transform* parent, std::string settingsName, std::string currentValue, std::function<void(std::string)> onValueChange) {
        return CreateStringSetting(parent, settingsName, currentValue, UnityEngine::Vector2(0.0f, 0.0f), onValueChange);
    }

    InputFieldView* CreateStringSetting(Transform* parent, std::string settingsName, std::string currentValue, UnityEngine::Vector2 anchoredPosition, std::function<void(std::string)> onValueChange) {
        return CreateStringSetting(parent, settingsName, currentValue, anchoredPosition, UnityEngine::Vector3(1337.0f, 1337.0f, 1337.0f), onValueChange);
    }
    
    InputFieldView* CreateStringSetting(Transform* parent, std::string settingsName, std::string currentValue, UnityEngine::Vector2 anchoredPosition, UnityEngine::Vector3 keyboardPositionOffset, std::function<void(std::string)> onValueChange) {
        InputFieldView* originalFieldView = ArrayUtil::First(Resources::FindObjectsOfTypeAll<InputFieldView*>(), [](InputFieldView* x) { 
            return to_utf8(csstrtostr(x->get_name())) == "GuestNameInputField"; });
        GameObject* gameObj = Object::Instantiate(originalFieldView->get_gameObject(), parent, false);
        static auto name = il2cpp_utils::createcsstr("QuestUIStringSetting", il2cpp_utils::StringType::Manual);
        gameObj->set_name(name);
        LayoutElement* layoutElement = gameObj->AddComponent<LayoutElement*>();
        layoutElement->set_preferredWidth(90.0f);
        layoutElement->set_preferredHeight(8.0f);
        gameObj->GetComponent<RectTransform*>()->set_anchoredPosition(anchoredPosition);

        InputFieldView* fieldView = gameObj->GetComponent<InputFieldView*>();
        fieldView->useGlobalKeyboard = true;
        fieldView->textLengthLimit = 128;
        fieldView->keyboardPositionOffset = keyboardPositionOffset;

        fieldView->Awake();

        Object::Destroy(fieldView->placeholderText->GetComponent<LocalizedTextMeshProUGUI*>());
        fieldView->placeholderText->GetComponent<TextMeshProUGUI*>()->SetText(il2cpp_utils::createcsstr(settingsName));
        fieldView->SetText(il2cpp_utils::createcsstr(currentValue));
        fieldView->onValueChanged = InputFieldView::InputFieldChanged::New_ctor();
        if(onValueChange) {
            fieldView->onValueChanged->AddListener(MakeDelegate(UnityAction_1<InputFieldView*>*,
                (std::function<void(InputFieldView*)>)[onValueChange](InputFieldView* fieldView){
                    onValueChange(to_utf8(csstrtostr(fieldView->get_text())));
                }));
        }
        return fieldView;
    }

    SimpleTextDropdown* CreateDropdown(Transform* parent, std::string dropdownName, std::string currentValue, std::vector<std::string> values, std::function<void(std::string)> onValueChange) {
        GameObject* gameObj = Object::Instantiate(dropdownListPrefab, parent, false);
        static auto name = il2cpp_utils::createcsstr("QuestUIDropdownList", il2cpp_utils::StringType::Manual);
        gameObj->set_name(name);
        SimpleTextDropdown* dropdown = gameObj->GetComponentInChildren<SimpleTextDropdown*>();
        dropdown->get_gameObject()->SetActive(false);

        auto* physicsRaycaster = GetPhysicsRaycasterWithCache();
        if(physicsRaycaster)
            reinterpret_cast<VRGraphicRaycaster*>(dropdown->GetComponentInChildren(csTypeOf(VRGraphicRaycaster*), true))->physicsRaycaster = physicsRaycaster;
        
        reinterpret_cast<ModalView*>(dropdown->GetComponentInChildren(csTypeOf(ModalView*), true))->container = GetDiContainer();

        static auto labelName = il2cpp_utils::createcsstr("Label", il2cpp_utils::StringType::Manual);
        GameObject* labelObject = gameObj->get_transform()->Find(labelName)->get_gameObject();
        GameObject::Destroy(labelObject->GetComponent<LocalizedTextMeshProUGUI*>());
        labelObject->GetComponent<TextMeshProUGUI*>()->SetText(il2cpp_utils::createcsstr(dropdownName));

        LayoutElement* layoutElement = gameObj->AddComponent<LayoutElement*>();
        layoutElement->set_preferredWidth(90.0f);
        layoutElement->set_preferredHeight(8.0f);

        List<Il2CppString*>* list = List<Il2CppString*>::New_ctor();
        int selectedIndex = 0;
        for(int i = 0; i < values.size(); i++){
            std::string value = values[i];
            if(currentValue == value)
                selectedIndex = i;
            list->Add(il2cpp_utils::createcsstr(value));
        }
        dropdown->SetTexts(reinterpret_cast<System::Collections::Generic::IReadOnlyList_1<Il2CppString*>*>(list));
        dropdown->SelectCellWithIdx(selectedIndex);

        if(onValueChange) {
            using DelegateType = System::Action_2<DropdownWithTableView*, int>*;
            dropdown->add_didSelectCellWithIdxEvent(MakeDelegate(DelegateType,
                (std::function<void(SimpleTextDropdown*, int)>)[onValueChange](SimpleTextDropdown* dropdownWithTableView, int index){
                    onValueChange(to_utf8(csstrtostr(reinterpret_cast<List<Il2CppString*>*>(dropdownWithTableView->texts)->get_Item(index))));
                }));
        }

        dropdown->get_gameObject()->SetActive(true);
        gameObj->SetActive(true);
        return dropdown;
    }
    
    GameObject* CreateFloatingScreen(UnityEngine::Vector2 screenSize, UnityEngine::Vector3 position, UnityEngine::Vector3 rotation, float curvatureRadius, bool hasBackground, bool createHandle, int handleSide) {
        //Set up canvas components
        auto gameObject = CreateCanvas();
        auto manager = gameObject->AddComponent<FloatingScreenManager*>();
        auto screen = gameObject->AddComponent<FloatingScreen*>();
        if(createHandle) {
            screen->set_showHandle(true);
            screen->set_side(handleSide);
        }
        auto curvedCanvasSettings = gameObject->AddComponent<CurvedCanvasSettings*>();
        curvedCanvasSettings->SetRadius(curvatureRadius);

        //background
        if(hasBackground) {
            static auto backgroundGoName = il2cpp_utils::createcsstr("bg", il2cpp_utils::StringType::Manual);
            auto backgroundGo = UnityEngine::GameObject::New_ctor(backgroundGoName);
            backgroundGo->get_transform()->SetParent(gameObject->get_transform(), false);
            auto background = backgroundGo->AddComponent<Backgroundable*>();
            static auto backgroundName = il2cpp_utils::createcsstr("round-rect-panel", il2cpp_utils::StringType::Manual);
            background->ApplyBackgroundWithAlpha(backgroundName, 0.5f);
            screen->set_bgGo(backgroundGo);
        }

        //Transform
        auto transform = gameObject->get_transform();
        transform->set_position(position);
        transform->set_eulerAngles(rotation);
        screen->set_screenSize(screenSize);

        return gameObject;
    }

    GameObject* CreateColorPicker(Transform* parent, std::string text, UnityEngine::Color defaultColor, std::function<void(UnityEngine::Color, ColorChangeUIEventType)> onValueChange) {
        // use QuestUI toggle as starting point to make positioning and sizing easier
        auto fakeToggle = CreateToggle(parent, text);
        auto gameObject = fakeToggle->get_transform()->get_parent()->get_gameObject();
        Object::Destroy(fakeToggle->get_gameObject());
        
        // create element that gets toggled to contain the actual picker
        static auto name = il2cpp_utils::createcsstr("QuestUIColorPickerModal", il2cpp_utils::StringType::Manual);
        auto pickerModalGO = GameObject::New_ctor(name);
        auto pickerModalGORect = pickerModalGO->AddComponent<RectTransform*>();
        pickerModalGORect->set_anchorMin(UnityEngine::Vector2(1.0f, 1.0f));
        pickerModalGORect->set_anchorMax(UnityEngine::Vector2(1.0f, 1.0f));
        pickerModalGORect->set_sizeDelta(UnityEngine::Vector2(40.0f, 40.0f));
        pickerModalGO->get_transform()->SetParent(parent, false);
        pickerModalGO->SetActive(false);

        // initialize the color button that toggles the picker open
        auto buttonBase = ArrayUtil::First(Resources::FindObjectsOfTypeAll<ColorPickerButtonController*>(), [](ColorPickerButtonController* x) { 
            return to_utf8(csstrtostr(x->get_name())) == "ColorPickerButtonSecondary"; });
        auto buttonGO = Object::Instantiate(buttonBase->get_gameObject(), gameObject->get_transform(), false);
        static auto buttonGOName = il2cpp_utils::createcsstr("QuestUIColorPickerButton", il2cpp_utils::StringType::Manual);
        buttonGO->set_name(buttonGOName);
        auto buttonRect = buttonGO->GetComponent<RectTransform*>();
        buttonRect->set_anchorMin(UnityEngine::Vector2(1.0f, 0.5f));
        buttonRect->set_anchorMax(UnityEngine::Vector2(1.0f, 0.5f));
        buttonRect->set_anchoredPosition(UnityEngine::Vector2(-1.0f, -0.5f));
        buttonRect->set_pivot(UnityEngine::Vector2(1.0f, 0.5f));

        auto colorPickerButtonController = buttonGO->GetComponent<ColorPickerButtonController*>();
        colorPickerButtonController->SetColor(defaultColor);

        // initialize the picker itself
        auto pickerBase = ArrayUtil::First(Resources::FindObjectsOfTypeAll<HSVPanelController*>(), [](HSVPanelController* x) { 
            return to_utf8(csstrtostr(x->get_name())) == "HSVColorPicker"; });
        auto pickerGO = Object::Instantiate(pickerBase->get_gameObject(), pickerModalGORect, false);
        static auto pickerGOName = il2cpp_utils::createcsstr("QuestUIColorPickerController", il2cpp_utils::StringType::Manual);
        pickerGO->set_name(pickerGOName);
        auto hsvPanelController = pickerGO->GetComponent<HSVPanelController*>();
        static auto searchName = il2cpp_utils::createcsstr("ColorPickerButtonPrimary", il2cpp_utils::StringType::Manual);
        Object::Destroy(pickerGO->get_transform()->Find(searchName)->get_gameObject());
        hsvPanelController->set_color(defaultColor);
        auto pickerRect = pickerGO->GetComponent<UnityEngine::RectTransform*>();
        pickerRect->set_pivot(UnityEngine::Vector2(0.25f, 0.77f));
        pickerRect->set_localScale(UnityEngine::Vector3(0.75f, 0.75f, 0.75f));

        // event bindings
        using DelegateType = System::Action_2<UnityEngine::Color, ColorChangeUIEventType>*;
        hsvPanelController->add_colorDidChangeEvent(MakeDelegate(DelegateType,
            ((std::function<void(UnityEngine::Color, ColorChangeUIEventType)>)[colorPickerButtonController, onValueChange](UnityEngine::Color color, ColorChangeUIEventType eventType) {
                colorPickerButtonController->SetColor(color);
                if(onValueChange)
                    onValueChange(color, eventType);
            })
        ));
        colorPickerButtonController->button->get_onClick()->AddListener(MakeDelegate(UnityAction*, 
            (std::function<void()>)[pickerModalGO]() {
                pickerModalGO->SetActive(!pickerModalGO->get_activeSelf());
            }
        ));
        ExternalComponents* externalComponents = buttonGO->AddComponent<ExternalComponents*>();
        externalComponents->Add(pickerModalGORect);
        return buttonGO;
    }

    ModalView* CreateModal(Transform* parent, UnityEngine::Vector2 sizeDelta, UnityEngine::Vector2 anchoredPosition, std::function<void(ModalView*)> onBlockerClicked, bool dismissOnBlockerClicked) {
        static auto name = il2cpp_utils::createcsstr("QuestUIModalPrefab", il2cpp_utils::StringType::Manual);

        // declare var
        ModalView* orig = modalPrefab->GetComponent<ModalView*>();
        
        // instantiate
        GameObject* modalObj = Object::Instantiate(modalPrefab, parent, false);
        
        modalObj->set_name(name);
        modalObj->SetActive(false);

        // get the modal
        ModalView* modal = modalObj->GetComponent<ModalView*>();

        // copy fields
        modal->presentPanelAnimations = orig->presentPanelAnimations;
        modal->dismissPanelAnimation = orig->dismissPanelAnimation;
        modal->container = GetDiContainer();
        modalObj->GetComponent<VRGraphicRaycaster*>()->physicsRaycaster = GetPhysicsRaycasterWithCache();

        // destroy unneeded objects
        Object::DestroyImmediate(modalObj->GetComponent<TableView*>());
        Object::DestroyImmediate(modalObj->GetComponent<ScrollRect*>());
        Object::DestroyImmediate(modalObj->GetComponent<ScrollView*>());
        Object::DestroyImmediate(modalObj->GetComponent<EventSystemListener*>());

        // destroy all children except background
        static Il2CppString* BGname = il2cpp_utils::newcsstr<il2cpp_utils::CreationType::Manual>("BG");
        int childCount = modal->get_transform()->get_childCount();
        for (int i = 0; i < childCount; i++) {
            auto* child = modal->get_transform()->GetChild(i)->GetComponent<RectTransform*>();

            if (child->get_gameObject()->get_name()->Equals(BGname)) {
                child->set_anchoredPosition(Vector2::get_zero());
                child->set_sizeDelta(Vector2::get_zero());
            }
            else {
                // yeet the child
                Object::Destroy(child->get_gameObject());
            }
        }

        // set recttransform data
        auto rect = modalObj->GetComponent<RectTransform*>();
        rect->set_anchorMin({0.5f, 0.5f});
        rect->set_anchorMax({0.5f, 0.5f});
        rect->set_sizeDelta(sizeDelta);
        rect->set_anchoredPosition(anchoredPosition);

        // add callback
        modal->add_blockerClickedEvent(MakeDelegate(System::Action*,
            ((std::function<void()>) [onBlockerClicked, modal, dismissOnBlockerClicked] () {
                if (onBlockerClicked)
                    onBlockerClicked(modal); 
                if (dismissOnBlockerClicked)
                    modal->Hide(true, nullptr);
            })
        ));
        return modal;
    }

    ModalView* CreateModal(Transform* parent, UnityEngine::Vector2 sizeDelta, std::function<void(ModalView*)> onBlockerClicked, bool dismissOnBlockerClicked) {
        return CreateModal(parent, sizeDelta, {0.0f, 0.0f}, onBlockerClicked, dismissOnBlockerClicked);
    }

    ModalView* CreateModal(Transform* parent, std::function<void(ModalView*)> onBlockerClicked, bool dismissOnBlockerClicked) {
        return CreateModal(parent, {30.0f, 40.0f}, {0.0f, 0.0f}, onBlockerClicked, dismissOnBlockerClicked);
    }

    ModalView* CreateModal(Transform* parent, bool dismissOnBlockerClicked) {
        return CreateModal(parent, {30.0f, 40.0f}, {0.0f, 0.0f}, nullptr, dismissOnBlockerClicked);
    }

    GameObject* CreateScrollableModalContainer(ModalView* modal) {
        Vector2 sizeDelta = modal->GetComponent<RectTransform*>()->get_sizeDelta();
        float width = sizeDelta.x;
        float height = sizeDelta.y;

        Transform* parent = modal->get_transform();
        GameObject* content = CreateScrollView(parent);

        ExternalComponents* externalComponents = content->GetComponent<ExternalComponents*>();
        RectTransform* scrollTransform = externalComponents->Get<RectTransform*>();

        scrollTransform->set_anchoredPosition({-2.5f, 0.0f});
        scrollTransform->set_sizeDelta({7.5f, 0.0f});

        VerticalLayoutGroup* layout = content->GetComponent<VerticalLayoutGroup*>();
        
        layout->set_childControlWidth(true);
        layout->set_childForceExpandWidth(true);

        auto layoutelem = content->AddComponent<LayoutElement*>();
        layoutelem->set_preferredWidth(width - 10.0f);

        static auto name = il2cpp_utils::createcsstr("QuestUICreateScrollableModalContainer", il2cpp_utils::StringType::Manual);

        scrollTransform->get_gameObject()->set_name(name);
        return content;
    }
}
