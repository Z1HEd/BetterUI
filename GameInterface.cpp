#include <4dm.h>
#include "auilib/auilib.h"
#include <glm/gtc/random.hpp>
#include "InventoryActions.h"
#include "InventorySorter.h"
#include "4DKeyBinds.h"
#include <fstream>

QuadRenderer qr{};
FontRenderer font{};
gui::Interface ui;
gui::Text healthText;
gui::TextInput craftSearchInput;


bool shiftHeldDown = false;
bool ctrlHeldDown = false;

unsigned int ctrlShiftCraftCount = 50;
unsigned int ctrlCraftCount = 10;
unsigned int shiftCraftCount = 4096;

std::string configPath;

void updateConfig(const std::string& path, const nlohmann::json& j)
{
	std::ofstream configFileO(path);
	if (configFileO.is_open())
	{
		configFileO << j.dump(4);
		configFileO.close();
	}
}

// Read config
$hook(void, StateIntro, init, StateManager& s)
{
	original(self, s);

	// initialize opengl stuff
	glewExperimental = true;
	glewInit();
	glfwInit();

	configPath = std::format("{}/config.json", fdm::getModPath(fdm::modID));

	nlohmann::json configJson
	{
		{ "ShiftCraftCount", shiftCraftCount},
		{ "CtrlShiftCraftCount", ctrlShiftCraftCount },
		{ "CtrlCraftCount", ctrlCraftCount}
	};

	if (!std::filesystem::exists(configPath))
	{
		updateConfig(configPath, configJson);
	}
	else
	{
		std::ifstream configFileI(configPath);
		if (configFileI.is_open())
		{
			configJson = nlohmann::json::parse(configFileI);
			configFileI.close();
		}
	}


	if (!configJson.contains("ShiftCraftCount"))
	{
		configJson["ShiftCraftCount"] = shiftCraftCount;
		updateConfig(configPath, configJson);
	}
	if (!configJson.contains("CtrlShiftCraftCount"))
	{
		configJson["CtrlShiftCraftCount"] = ctrlShiftCraftCount;
		updateConfig(configPath, configJson);
	}
	if (!configJson.contains("CtrlCraftCount"))
	{
		configJson["CtrlCraftCount"] = ctrlCraftCount;
		updateConfig(configPath, configJson);
	}

	shiftCraftCount = configJson["ShiftCraftCount"];
	ctrlShiftCraftCount = configJson["CtrlShiftCraftCount"];
	ctrlCraftCount = configJson["CtrlCraftCount"];
}
//Initialize stuff when entering world

void viewportCallback(void* user, const glm::ivec4& pos, const glm::ivec2& scroll)
{
	GLFWwindow* window = (GLFWwindow*)user;

	// update the render viewport
	int wWidth, wHeight;
	glfwGetWindowSize(window, &wWidth, &wHeight);
	glViewport(pos.x, wHeight - pos.y - pos.w, pos.z, pos.w);

	// create a 2D projection matrix from the specified dimensions and scroll position
	glm::mat4 projection2D = glm::ortho(0.0f, (float)pos.z, (float)pos.w, 0.0f, -1.0f, 1.0f);
	projection2D = glm::translate(projection2D, { scroll.x, scroll.y, 0 });

	// update all 2D shaders
	const Shader* textShader = ShaderManager::get("textShader");
	textShader->use();
	glUniformMatrix4fv(glGetUniformLocation(textShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);

	const Shader* tex2DShader = ShaderManager::get("tex2DShader");
	tex2DShader->use();
	glUniformMatrix4fv(glGetUniformLocation(tex2DShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);

	const Shader* quadShader = ShaderManager::get("quadShader");
	quadShader->use();
	glUniformMatrix4fv(glGetUniformLocation(quadShader->id(), "P"), 1, GL_FALSE, &projection2D[0][0]);
}

$hook(void, StateGame, init, StateManager& s)
{
	original(self, s);

	font = { ResourceManager::get("pixelFont.png"), ShaderManager::get("textShader") };

	qr.shader = ShaderManager::get("quadShader");
	qr.init();

	ui.window = s.window;
	ui.viewportCallback = viewportCallback;
	ui.viewportUser = s.window;
	ui.font = &font;
	ui.qr = &qr;

	healthText.text = "100";
	healthText.alignX(gui::ALIGN_LEFT);
	healthText.alignY(gui::ALIGN_BOTTOM);
	healthText.size = 2;
	healthText.shadow = true;
	healthText.offsetY(-26);

	craftSearchInput.height = 30;
	craftSearchInput.width = 300;
	craftSearchInput.alignX(gui::ALIGN_RIGHT);
	craftSearchInput.alignY(gui::ALIGN_TOP);
	int x, y;
	self->player.inventoryManager.craftingMenuBox.getPos(&ui, &x, &y);
	craftSearchInput.offsetY(y - craftSearchInput.height);
	craftSearchInput.editable = true;

	ui.addElement(&craftSearchInput);

	gui::Text* craftingText = &self->player.inventoryManager.craftingText;

	craftingText->offsetY(20);
}

// Make auilib work lol

$hook(void, StateGame, windowResize, StateManager& s, GLsizei width, GLsizei height) {
	original(self, s, width, height);
	viewportCallback(s.window, { 0,0, width, height }, { 0,0 });
}

//Render UI

$hook(void, Player, renderHud, GLFWwindow* window) {
	original(self, window);

	// Hp indicator
	static float timeSinceDamage;

	timeSinceDamage = glfwGetTime() - self->damageTime;


	static glm::vec4 textColor;
	if (self->health > 70) textColor = { .5,1,.5,1 }; //light green if high health
	else if (self->health > 30) textColor = { 1,0.64f,0,1 }; // orange if half health
	else textColor = { 1,0.1f,0.1f,1 }; // red if low health

	healthText.text = std::to_string((int)self->health);

	int width, height;
	healthText.getSize(&ui, &width, &height);

	glDisable(GL_DEPTH_TEST);
	if (timeSinceDamage < Player::DAMAGE_COOLDOWN) {

		glm::vec2 aOffset = glm::diskRand(5.f);
		glm::vec2 bOffset = glm::diskRand(5.f);

		healthText.color = { 1,0,1,1 };
		healthText.offsetX(42 - width / 2 + aOffset.x);
		healthText.offsetY(-26 + aOffset.y);
		healthText.render(&ui);

		healthText.color = { 1,0,1,1 };
		healthText.offsetX(42 - width / 2 + aOffset.x);
		healthText.offsetY(-26 + aOffset.y);
		healthText.render(&ui);

		healthText.offsetY(-26);
	}
	healthText.color = textColor;
	healthText.offsetX(42 - width / 2);
	healthText.render(&ui);

	if (self->inventoryManager.isOpen())
		ui.render();

	glEnable(GL_DEPTH_TEST);

	CraftingMenu* menu = &self->inventoryManager.craftingMenu;
	gui::ContentBox* craftingMenuBox = &self->inventoryManager.craftingMenuBox;
	int x, y;
	for (int i = 0; i < craftingMenuBox->elements.size();i++) {
		craftingMenuBox->elements[i]->getPos(craftingMenuBox->parent, &x, &y);
	}
}

//Recipe filtering

$hook(void, CraftingMenu, updateAvailableRecipes)
{
	original(self);
	for (auto it = self->craftableRecipes.begin(); it < self->craftableRecipes.end(); )
	{
		stl::string lowerItem = it->result->getName();
		std::transform(lowerItem.begin(), lowerItem.end(), lowerItem.begin(),
			[](uint8_t c) { return std::tolower(c); });

		stl::string lowerInput = craftSearchInput.text;
		std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(),
			[](uint8_t c) { return std::tolower(c); });

		if (lowerItem.find(lowerInput) == std::string::npos)
		{
			it = self->craftableRecipes.erase(it);
			continue;
		}
		it++;
	}
	self->Interface->updateCraftingMenuBox();
}

bool hotbarSwapInput(StateManager& s, int key) {
	if (key<GLFW_KEY_1 || key>GLFW_KEY_8) return false;

	double cursorX, cursorY;
	glfwGetCursorPos(s.window, &cursorX, &cursorY);

	Inventory* inventory = &StateGame::instanceObj.player.inventory;
	int cursorSlotIndex = inventory->getSlotIndex({ cursorX,cursorY });

	if (cursorSlotIndex < 0) {
		inventory = &StateGame::instanceObj.player.hotbar;
		cursorSlotIndex = inventory->getSlotIndex({ cursorX,cursorY });
	}

	if (cursorSlotIndex < 0) {
		inventory = &StateGame::instanceObj.player.equipment;
		cursorSlotIndex = inventory->getSlotIndex({ cursorX,cursorY });
	}

	if (cursorSlotIndex < 0 && StateGame::instanceObj.player.inventoryManager.secondary) {
		inventory = StateGame::instanceObj.player.inventoryManager.secondary;
		cursorSlotIndex = inventory->getSlotIndex({ cursorX,cursorY });
	}

	if (cursorSlotIndex < 0) return false;


	int hotbarSlotIndex = key - GLFW_KEY_1;

	if (inventory == &StateGame::instanceObj.player.hotbar && hotbarSlotIndex == cursorSlotIndex) return true;

	InventoryManager* manager = &StateGame::instanceObj.player.inventoryManager;

	InventoryActions::swapIndex(manager,
		inventory,
		&StateGame::instanceObj.player.hotbar,
		cursorSlotIndex,
		hotbarSlotIndex, &StateGame::instanceObj.player.hotbar);

	return true;
}

// Fastcrafting
$hook(bool, CraftingMenu, craftRecipe, int recipeIndex) {
	if (StateGame::instanceObj.world->getType() != World::TYPE_SINGLEPLAYER) return original(self, recipeIndex);

	if (!original(self, recipeIndex)) return false;
	unsigned int fastcraftCount = 1;

	if (shiftHeldDown && ctrlHeldDown)
		fastcraftCount = ctrlShiftCraftCount;
	else if (shiftHeldDown)
		fastcraftCount = shiftCraftCount;
	else if (ctrlHeldDown)
		fastcraftCount = ctrlCraftCount;

	for (unsigned int i = 0; i < fastcraftCount - 1; i++) {
		if (!original(self, recipeIndex)) break;
	}

	return true;
}
$hookStatic(bool, InventoryManager, craftingMenuCallback, int recipeIndex, void* user) {

	unsigned int fastcraftCount = 1;

	if (shiftHeldDown && ctrlHeldDown)
		fastcraftCount = ctrlShiftCraftCount;
	else if (shiftHeldDown)
		fastcraftCount = shiftCraftCount;
	else if (ctrlHeldDown)
		fastcraftCount = ctrlCraftCount;

	auto dummyItem = Item::create(CraftingMenu::recipes[recipeIndex]["result"]["name"].get<std::string>(), 1);

	fastcraftCount = std::min(fastcraftCount, dummyItem->getStackLimit() /
		CraftingMenu::recipes[recipeIndex]["result"]["count"]);

	for (unsigned int i = 0; i < fastcraftCount - 1; i++) {
		original(recipeIndex, user);
	}

	return original(recipeIndex, user);
}


// Keybinds

void sortInventory(GLFWwindow* window, int action, int mods) {
	if (action != GLFW_PRESS) return;

	InventoryManager* manager = &StateGame::instanceObj.player.inventoryManager;
	if (!manager->isOpen()) return;
	if (manager->secondary->name == "inventoryAndEquipment")
		InventorySorter::sort(manager, (InventoryGrid*)((InventoryPlayer*)manager->secondary)->hotbar, (InventoryGrid*)manager->primary);
	else
		InventorySorter::sort(manager, (InventoryGrid*)manager->secondary,(Inventory*)manager->primary);
}

void swapHands(GLFWwindow* window, int action, int mods) {
	if (action != GLFW_PRESS || craftSearchInput.active) return;

	InventoryManager* manager = &StateGame::instanceObj.player.inventoryManager;

	InventoryActions::swapIndex(manager,
		&StateGame::instanceObj.player.equipment,
		&StateGame::instanceObj.player.hotbar,
		0,
		StateGame::instanceObj.player.hotbar.selectedIndex, &StateGame::instanceObj.player.hotbar);

}

$hook(bool, Player, keyInput, GLFWwindow* window, World* world, int key, int scancode, int action, int mods)
{
	if (!KeyBinds::isLoaded())
	{
		if (key == GLFW_KEY_R)
			sortInventory(window, action, mods);
		if (key == GLFW_KEY_F)
			swapHands(window, action, mods);
	}
	return original(self, window, world, key, scancode, action, mods);
}
$exec
{
	KeyBinds::addBind("BetterUX", "Sort Inventory", glfw::Keys::R, KeyBindsScope::PLAYER, sortInventory);
	KeyBinds::addBind("BetterUX", "Swap Hands", glfw::Keys::F, KeyBindsScope::PLAYER, swapHands);
}

// Passing inputs into UI

$hook(void, StateGame, charInput, StateManager& s, uint32_t codepoint)
{
	if (!self->player.inventoryManager.isOpen() || !craftSearchInput.active || !ui.charInput(codepoint))
		return original(self, s, codepoint);
	self->player.inventoryManager.craftingMenu.updateAvailableRecipes();
}
$hook(void, StateGame, keyInput, StateManager& s, int key, int scancode, int action, int mods)
{
	// used for multicrafting
	if (action != GLFW_REPEAT)
	{
		switch (key)
		{
		case GLFW_KEY_LEFT_SHIFT:
		{
			shiftHeldDown = action == GLFW_PRESS;
		} break;
		case GLFW_KEY_LEFT_CONTROL:
		{
			ctrlHeldDown = action == GLFW_PRESS;
		} break;
		}
	}

	if (self->player.inventoryManager.isOpen() && action == GLFW_PRESS && hotbarSwapInput(s, key))
		return;
	if (!self->player.inventoryManager.isOpen() || !craftSearchInput.active || key == GLFW_KEY_ESCAPE)
		return original(self, s, key, scancode, action, mods);
	if (ui.keyInput(key, scancode, action, mods))
		self->player.inventoryManager.craftingMenu.updateAvailableRecipes();
}
$hook(void, StateGame, mouseInput, StateManager& s, double xpos, double ypos)
{
	original(self, s, xpos, ypos);
	if (!self->player.inventoryManager.isOpen()) return;
	ui.mouseInput(xpos, ypos);
}
$hook(void, StateGame, mouseButtonInput, StateManager& s, int button, int action, int mods)
{
	if (!self->player.inventoryManager.isOpen() || !ui.mouseButtonInput(button, action, mods))
		original(self, s, button, action, mods);
	//updateFilteredCrafts(&self->player);
}
$hook(void, StateGame, scrollInput, StateManager& s, double xoffset, double yoffset)
{
	original(self, s, xoffset, yoffset);
	if (!self->player.inventoryManager.isOpen()) return;
	ui.scrollInput(xoffset, yoffset);
}
