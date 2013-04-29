/**
**/

#ifndef LOVE_WINDOW_PPAPI_WINDOW_H
#define LOVE_WINDOW_PPAPI_WINDOW_H

// LOVE
#include <common/Matrix.h>
#include <window/Window.h>

namespace pp {
class Graphics3D;
}  // namespace pp

namespace love
{
namespace window
{
namespace ppapi
{
	class Window : public love::window::Window
	{
	private:
		pp::Graphics3D* graphics_3d;
		int width;
		int height;
		int contextWidth;
		int contextHeight;
		int screenWidth;
		int screenHeight;
		bool created;
		bool focused;

		Matrix screenToWindowMatrix;

		bool createContext(int width, int height);

	public:
		Window(int screenWidth, int screenHeight);
		~Window();

		bool setWindow(int width = 800, int height = 600, bool fullscreen = false, bool vsync = true, int fsaa = 0);
		void getWindow(int &width, int &height, bool &fullscreen, bool &vsync, int &fsaa);

		bool checkWindowSize(int width, int height, bool fullscreen);
		WindowSize **getFullscreenSizes(int &n);

		int getWidth();
		int getHeight();

		int getScreenWidth();
		int getScreenHeight();

		bool isCreated();

		void setWindowTitle(std::string &title);
		std::string getWindowTitle();

		bool setIcon(love::image::ImageData *imgd);

		void swapBuffers();

		bool hasFocus();
		void setMouseVisible(bool visible);
		bool getMouseVisible();

		void onScreenChanged(int width, int height);
		void onFocusChanged(bool has_focus);

		void screenToWindow(int x, int y, int &out_x, int &out_y);

		static love::window::Window *getSingleton();

		const char *getName() const;
	}; // Window
} // ppapi
} // window
} // love

#endif // LOVE_WINDOW_PPAPI_WINDOW_H

