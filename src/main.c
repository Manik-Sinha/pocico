/*
pocico is a game about changing states.
Copyright (C) 2018 Manik Sinha

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

Official website: https://manik-sinha.itch.io/pocico
Official repository: https://github.com/Manik-Sinha/pocico
Official email: ManikSinha@protonmail.com
*/
#include <GL/glew.h>
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "nanovg.h"
#define NANOVG_GL2_IMPLEMENTATION
#include "nanovg_gl.h"

char build_number_string[] = "Build Number 1\nEarly Access February 11, 2018";

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

//char build_number[4] = "1   ";
//char build_number_string[] = "Build Number     \nEarly Access February 11, 2018";

//Pointer to our uninitialized window.
SDL_Window * window = NULL;

//Pointer to our uninitialized renderer.
SDL_Renderer * renderer = NULL;

//Pointer to the opengl context.
SDL_GLContext * gl_context = NULL;

//Flag idicating whether the game is running.
bool game_is_running = true;

//The golden ratio.
const float GOLDEN_RATIO = 1.61803398875f;

//1 / golden ratio.
const float INVERSE_GOLDEN_RATIO = 0.61803398875f;

//Free all memory and properly shutdown SDL.
static inline void cleanup();

typedef struct Vertex {
  float x;
  float y;
} Vertex;

typedef struct Game {
  const int uid;
  const int number_of_states;
  int * left_state;
  int * right_state;
  int mod;
  const int * move_matrix_index;
  const int * move_matrix;
  void (*init) (struct Game*);
  void (*draw) (NVGcontext * vg, struct Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision);
} Game;

//Triforce functions.
void draw_triforce(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision);

//Foursquare functions.
void draw_foursquare(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision);

//Squarediamond functions.
void draw_squarediamond(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision);

//Amman Beenker functions.
void draw_ammann_beenker(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision);

//Check if two colors are the same.
static inline bool same_color(SDL_Color c1, SDL_Color c2);

//Randomize colors.
static inline void randomize_colors(SDL_Color * colors, int size);

//Check if a point is in a square. The point to check is (mX, mY).
static inline bool point_in_square(float mX, float mY, float x, float y, float s);

//Check if point (x,y) is in a triangle. This function expects points to be in counter-clockwise order.
static bool point_in_triangle(float x, float y, float x1, float y1, float x2, float y2, float x3, float y3);

//Check if point (x, y) is in a quadrilateral. This function expects points to be in counter-clockwise order.
static bool point_in_quad(float x, float y, float p0x, float p0y, float p1x, float p1y, float p2x, float p2y, float p3x, float p3y);

//Check if point (x, y) is in rect defined by x, y, w, h.
static bool point_in_rect(float mX, float mY, float x, float y, float w, float h);

//Used specifically to draw the face of the randomize state and randomize color dice.
static void draw_die_face(NVGcontext * vg, float x, float y, float width, float height, float radius, int face, const NVGcolor * const color);

enum GAMESTATE {MAIN_MENU, PLAYING};

//Transform the state of one side of a game based on which position the player
//clicked on.
static inline void
transform(
  const int position,
  int * const state,
  const int * const move_matrix,
  const int * const move_matrix_index,
  const int mod,
  const int times
)
{
  int index = move_matrix_index[position];
  int number_of_states_to_modify = move_matrix[index];
  int first_state = index + 1;
  for(int i = first_state; i < (first_state + number_of_states_to_modify); i++)
  {
    state[move_matrix[i]] = (state[move_matrix[i]] + times) % mod;
  }
}

//Check whether two states match or not.
static inline bool matching(
  const int * const left_state,
  const int * const right_state,
  int number_of_states
)
{
  for(int i = 0; i < number_of_states; i++)
  {
    if(left_state[i] != right_state[i])
    {
      return false;
    }
  }
  return true;
}

//Randomize the left and right states of a game.
static void randomize(Game * game)
{
  int old_left_state[game->number_of_states];
  int old_right_state[game->number_of_states];
  for(int i = 0; i < game->number_of_states; i++)
  {
    old_left_state[i] = game->left_state[i];
    old_right_state[i] = game->right_state[i];
  }

  bool won = matching(game->left_state, game->right_state, game->number_of_states);

  while(true)
  {
    //Randomize the left state, and
    //set right state to be exactly left state.
    for(int i = 0; i < game->number_of_states; i++)
    {
      game->left_state[i] = rand() % game->mod;
      game->right_state[i] = game->left_state[i];
    }

    //Randomize the right state by randomly applying the move_matrix on it.
    for(int i = 0; i < game->number_of_states; i++)
    {
      int times = rand() % game->number_of_states;
      transform(
        i,
        game->right_state,
        game->move_matrix,
        game->move_matrix_index,
        game->mod,
        times
      );
    }

    //If the player won, then we are done if left and right states aren't the same.
    if(won)
    {
      if(!matching(game->left_state, game->right_state, game->number_of_states))
      {
        return;
      }
    }
    else
    {//However, if the player hasn't won, then we are more picky.
      //First, the left and right states have to be different,
      if(!matching(game->left_state, game->right_state, game->number_of_states))
      {
        //and then, either the left state has to be different
        //than what it was before randomizing, or the right has to be different
        //than what it was before randomizing. This ensures that when we randomize,
        //there is a visible change. Otherwise there is a possibility that
        //exactly the same state as before randomizing occurs again.
        if( (!matching(game->left_state, old_left_state, game->number_of_states)) ||
            (!matching(game->right_state, old_right_state, game->number_of_states)) )
        {
          return;
        }
      }
    }
  }
}


void standard_init(Game * game)
{
  randomize(game);
}

int game_01_triforce_left_state[4];
int game_01_triforce_right_state[4];
const int game_01_triforce_move_matrix_index[] = {0, 4, 7, 10};
const int game_01_triforce_move_matrix[] = {
  3, 1, 2, 3, //Center
  2, 0, 1,    //Top
  2, 0, 2,    //Left
  2, 0, 3,    //Right
};

int game_02_foursquare_left_state[4];
int game_02_foursquare_right_state[4];
const int game_02_foursquare_move_matrix_index[] = {0, 4, 8, 12};
const int game_02_foursquare_move_matrix[] = {
  3, 0, 1, 3, //Top left
  3, 0, 1, 2, //Top right
  3, 1, 2, 3, //Bottom right
  3, 0, 2, 3, //Bottom left
};

int game_03_squarediamond_left_state[24];
int game_03_squarediamond_right_state[24];
const int game_03_squarediamond_move_matrix_index[] = {
  0, 4, 8, 13, 18, 22,
  26, 30, 35, 41, 47, 52,
  56, 60, 65, 71, 77, 82,
  86, 90, 94, 99, 104, 108
};
const int game_03_squarediamond_move_matrix[] = {
  3, 0, 1, 11,          //0
  3, 1, 0, 2,           //1
  4, 2, 1, 3, 9,        //2
  4, 3, 2, 4, 8,        //3
  3, 4, 3, 5,           //4
  3, 5, 4, 6,           //5
  3, 6, 5, 7,           //6
  4, 7, 6, 8, 16,       //7
  5, 8, 3, 7, 9, 15,    //8
  5, 9, 2, 8, 10, 14,   //9
  4, 10, 9, 11, 13,     //10
  3, 11, 0, 10,         //11
  3, 12, 13, 23,        //12
  4, 13, 10, 12, 14,    //13
  5, 14, 9, 13, 15, 21, //14
  5, 15, 8, 14, 16, 20, //15
  4, 16, 7, 15, 17,     //16
  3, 17, 16, 18,        //17
  3, 18, 17, 19,        //18
  3, 19, 18, 20,        //19
  4, 20, 15, 19, 21,    //20
  4, 21, 14, 20, 22,    //21
  3, 22, 21, 23,        //22
  3, 23, 12, 22,        //23
};

//https://en.wikipedia.org/wiki/Ammann–Beenker_tiling
int game_04_ammann_beenker_left_state[24];
int game_04_ammann_beenker_right_state[24];

const int game_04_ammann_beenker_move_matrix_index[] = {
  0, 6, 12, 16, 20, 26, 32, 36, 42, 46, 52, 56, 62, 66, 72, 76, 80, 86, 92, 98, 104, 110, 116, 122
};
const int game_04_ammann_beenker_move_matrix[] = {
  5, 0, 2, 15, 16, 17,   //0
  5, 1, 2, 3, 17, 18,    //1
  3, 0, 1, 2,            //2
  3, 1, 3, 4,            //3
  5, 3, 4, 6, 18, 19,    //4
  5, 5, 6, 8, 19, 20,    //5
  3, 4, 5, 6,            //6
  5, 7, 8, 10, 20, 21,   //7
  3, 5, 7, 8,            //8
  5, 9, 10, 12, 21, 22,  //9
  3, 7, 9, 10,           //10
  5, 11, 12, 14, 22, 23, //11
  3, 9, 11, 12,          //12
  5, 13, 14, 15, 16, 23, //13
  3, 11, 13, 14,         //14
  3, 0, 13, 15,          //15
  5, 0, 13, 16, 17, 23,  //16
  5, 0, 1, 16, 17, 18,   //17
  5, 1, 4, 17, 18, 19,   //18
  5, 4, 5, 18, 19, 20,   //19
  5, 5, 7, 19, 20, 21,   //20
  5, 7, 9, 20, 21, 22,   //21
  5, 9, 11, 21, 22, 23,  //22
  5, 11, 13, 16, 22, 23 //23
};

Game game_triforce = {
  1, //uid
  4, //number of states
  game_01_triforce_left_state,
  game_01_triforce_right_state,
  2, //mod
  game_01_triforce_move_matrix_index,
  game_01_triforce_move_matrix,
  standard_init,
  draw_triforce
};
Game game_foursquare = {
  2, //uid
  4, //number of states
  game_02_foursquare_left_state,
  game_02_foursquare_right_state,
  2, //mod
  game_02_foursquare_move_matrix_index,
  game_02_foursquare_move_matrix,
  standard_init,
  draw_foursquare
};

Game game_squarediamond = {
  3,  //uid
  24, //number of states
  game_03_squarediamond_left_state,
  game_03_squarediamond_right_state,
  2,  //mod
  game_03_squarediamond_move_matrix_index,
  game_03_squarediamond_move_matrix,
  standard_init,
  draw_squarediamond
};

Game game_ammann_beenker = {
  4,  //uid
  24, //number of states
  game_04_ammann_beenker_left_state,
  game_04_ammann_beenker_right_state,
  2,  //mod
  game_04_ammann_beenker_move_matrix_index, //move matrix index
  game_04_ammann_beenker_move_matrix, //move matrix
  standard_init,
  draw_ammann_beenker
};

#define GAME_COUNT 4
Game * games[GAME_COUNT] = {
  &game_triforce,
  &game_foursquare,
  &game_squarediamond,
  &game_ammann_beenker,
};

int main(int argc, char * argv[])
{
  printf("In main.\n");

  //Seed rng. Later I will try to use PCG as psuedo random number generator.
  srand(time(0));

  //Initialize SDL.
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
  {
    printf("SDL_Init failed: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
  {
    printf("SDL_mixer could not initialize! %s\n", Mix_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  //TODO: test high-dpi
  //Make our window.
  window = SDL_CreateWindow(
    "pocico",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    DEFAULT_WIDTH,//312,//960,
    DEFAULT_HEIGHT,//250,//540,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI// | SDL_WINDOW_FULLSCREEN_DESKTOP
  );

  //Check if our window initialized.
  if(window == NULL)
  {
    printf("Error: Failed to initialize window! %s\n", SDL_GetError());
    //Shut down SDL.
    cleanup();
    return EXIT_FAILURE;
  }

  //SDL_SetWindowMinimumSize(window, 240, 135);

  gl_context = SDL_GL_CreateContext(window);
  if(gl_context == NULL)
  {
    printf("Error: Failed to create opengl context! %s\n", SDL_GetError());
    cleanup();
    return EXIT_FAILURE;
  }

  //Initialize GLEW.
  glewExperimental = GL_TRUE;
  GLenum glew_error = glewInit();
  if(glew_error != GLEW_OK)
  {
    printf("Error: Failed to initialize GLEW! %s\n", glewGetErrorString(glew_error));
    cleanup();
    return EXIT_FAILURE;
  }

  //Try to use Vsync
  if(SDL_GL_SetSwapInterval(1) < 0)
  {
    printf("Warning: Unable to use vsync! %s\n", SDL_GetError());
  }

/*
  //TODO: If neccessary, write code to use the software renderer if the
  //      accelerated renderer fails.
  //renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  //Check if we successfully initialized the renderer.
  if(renderer == NULL)
  {
    printf("Error: Failed to initialize renderer! %s\n", SDL_GetError());
    //Shutdown SDL.
    cleanup();
    return EXIT_FAILURE;
  }
*/

  NVGcontext * vg = NULL;
  vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);

  {
      //GL_STENCIL_BITS doesnt work for opengl 3.3
      GLint stencil_bits = 0;
      glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
      if(stencil_bits < 1)
      {
        printf("no stencil buffer!\n");
        vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_DEBUG);
      }
      else
      {
        vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
      }
      printf("stencil bits %d\n", stencil_bits);
  }

  if(vg == NULL)
  {
    printf("Error: could not initialize nanovg!\n");
    cleanup();
    return EXIT_FAILURE;
  }

  //Variable to handle events.
  SDL_Event event;

  //Used to store the mouse position.
  SDL_Point mouse = {0, 0};

  //The current width and height of the window.
  int width = DEFAULT_WIDTH;//312;//960;
  int height = DEFAULT_HEIGHT;//250;//540;

  for(int i = 0; i < GAME_COUNT; i++)
  {
    games[i]->init(games[i]);
  }

  int current_game = 0;

  //This flag applies to the current game.
  bool won_game = false;

  //Colors.
  #define MAX_COLORS 9
  static SDL_Color colors[MAX_COLORS] = {
    {170, 10, 60},
    {252, 122, 82},
    {240, 240, 50},
    {160, 250, 130},
    {20, 210, 220},
    {0, 160, 250},
    {0, 90, 200},
    {130, 20, 160},
    {250, 120, 250},
  };
  //randomize_colors(colors, MAX_COLORS);

  //Fonts.
  int font_roboto_regular = nvgCreateFont(vg, "sans", "./fonts/Roboto-Regular.ttf");
  if(font_roboto_regular == -1)
  {
    printf("Error: could not load font!\n");
    //TODO: popup a dialog box here and warn the player that fonts couldn't be loaded.
  }

  //Sound effects and Music.
  #define MAX_NOTES 15
  Mix_Chunk * notes[MAX_NOTES] = {
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
  };

  char * notes_paths[MAX_NOTES] = {
    "./sfx/C.wav",
    "./sfx/D.wav",
    "./sfx/E.wav",
    "./sfx/F.wav",
    "./sfx/G.wav",
    "./sfx/A.wav",
    "./sfx/B.wav",
    "./sfx/C_high.wav",
    "./sfx/D_high.wav",
    "./sfx/E_high.wav",
    "./sfx/F_high.wav",
    "./sfx/G_high.wav",
    "./sfx/A_high.wav",
    "./sfx/B_high.wav",
    "./sfx/C_high_high.wav"
  };

  //Load note sound effects.
  for(int i = 0; i < MAX_NOTES; i++)
  {
    notes[i] = Mix_LoadWAV(notes_paths[i]);
    if(notes[i] == NULL)
    {
      printf("Failed to load %s!\n", notes_paths[i]);
      //TODO: popup dialog box warning player the sfx couldn't be loaded.
    }
  }

  //Use 16 channels.
  Mix_AllocateChannels(100);

  //Win Messages.
  #define MAX_WIN_MESSAGES 8
  char * win_messages[MAX_WIN_MESSAGES] = {"You Win!", "Excellent!", "Good Job!", "Congratulations!", "Well Done!", "Superb!", "Success!", "Magnificent!"};
  char * current_win_message = win_messages[rand()%MAX_WIN_MESSAGES];

  //The game starts at the menu.
  enum GAMESTATE gamestate = MAIN_MENU;

  int randomize_state_die_face = rand() % 6 + 1;
  int randomize_color_die_face = rand() % 6 + 1;

  //Game loop.
  while(game_is_running)
  {
    //Gather input.
    //Update game state.
    //Draw game.

    //A flag checking if the mouse was pressed.
    bool mouse_button_down = false;
    //A flag checking if the mouse was released.
    bool mouse_button_up = false;

    //A flag checking if the escape key was pressed.
    bool escape_pressed = false;

    bool print_screen_pressed = false;
    //Gather input.
    //Handle events while they are on the queue.
    while(SDL_PollEvent(&event))
    {
      /*if(print_screen_pressed)
      {
        printf("event type %d\n", event.type);
        if(event.type == SDL_WINDOWEVENT)
        {
          printf("window event %d\n", event.window.event);
        }
      }*/
      switch(event.type)
      {
        case SDL_KEYDOWN:
          switch(event.key.keysym.sym)
          {
            case SDLK_LEFT:
              current_game--;
              if(current_game < 0) current_game = GAME_COUNT - 1;
              break;
            case SDLK_RIGHT:
              current_game++;
              if(current_game >= GAME_COUNT) current_game = 0;
              break;
            case SDLK_r:
              //randomize_colors(colors, MAX_COLORS);
              break;
            case SDLK_ESCAPE:
              escape_pressed = true;
              break;
            case SDLK_PRINTSCREEN:
              //printf("printscreen\n");
              //print_screen_pressed = true;
              break;
            default:
              break;
          }
          break;
        case SDL_MOUSEMOTION:
          //mouse_moved = true;
          mouse.x = event.motion.x;
          mouse.y = event.motion.y;
          break;
        case SDL_MOUSEBUTTONDOWN:
          mouse_button_down = true;
          break;
        case SDL_MOUSEBUTTONUP:
          mouse_button_up = true;
          break;
        case SDL_QUIT:
          game_is_running = false;
          break;

        //Handle window events.
        case SDL_WINDOWEVENT:
          switch(event.window.event)
          {
            //The window was resized.
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
              width = event.window.data1;
              height = event.window.data2;
              SDL_GL_GetDrawableSize(window, &width, &height); //For high dpi
              break;
          }
          break;
      }
    }

    glViewport(0, 0, width, height);
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    if(gamestate == MAIN_MENU)
    {
      nvgBeginFrame(vg, width, height, 1);

      //pocico logo.
      {
        float font_size = width * 0.125f;
        float test_height = height * 0.22222222f;
        if(test_height < font_size)
        {
          font_size = test_height;
        }
        nvgFontSize(vg, font_size);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        float start = 0.3125f;
        float spacing = 0.075f;
        const char * pocico[6] = {"p", "o", "c", "i", "c", "o"};
        for(int i = 0; i < 6; i++)
        {
          nvgFillColor(vg, nvgRGB(colors[i].r, colors[i].g, colors[i].b));
          nvgText(vg, width * (start + spacing * i), 0, pocico[i], NULL);
        }
      }

      //Copyright and version information.
      {
        const char * copyright = "Copyright 2018 Manik Sinha ";
        float font_size = width * 0.025f;
        float test_height = height * 0.1f;//16.0f / 9.0f * 0.025f * height;
        if(test_height < font_size)
        {
          font_size = test_height;
        }
        float x = width;
        float y = height - font_size;
        nvgFontSize(vg, font_size);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgText(vg, x, y, copyright, NULL);

        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        //build_number_string is a global variable.
        //char build_number_string[] = "Build Number     \nEarly Access Month Day, Year";
        /*build_number_string[13] = build_number[0];
        build_number_string[14] = build_number[1];
        build_number_string[15] = build_number[2];
        build_number_string[16] = build_number[3];*/
        nvgTextBox(vg, 0, height - font_size, width, build_number_string, NULL);
      }

      float play_button_x = 0.0f;
      float play_button_y = 0.0f;
      float play_button_bounds_width = 0.0f;
      float play_button_bounds_height = 0.0f;
      //Play Button.
      {
        float font_size = width * 0.05f;//0.025f;
        float test_height = height * 0.1f;
        if(test_height < font_size)
        {
          font_size = test_height;
        }
        play_button_x = width / 2.0f;
        //play_button_y = height/ 2.75f;
        play_button_y = height / 2.5f;
        float x = play_button_x;
        float y = play_button_y;
        nvgFontSize(vg, font_size);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);

        float bounds[4];
        nvgTextBounds(vg, x, y, "Play", NULL, bounds);
        float bounds_x = bounds[0];
        float bounds_y = bounds[1];
        float bounds_width = bounds[2] - bounds[0];
        float bounds_height = bounds[3] - bounds[1];

        float bounds_percent_x = 1.0f;
        bounds_x = bounds_x - bounds_width * bounds_percent_x * 0.5f;
        bounds_width = bounds_width + bounds_width * bounds_percent_x;

        float bounds_percent_y = 0.5625f;
        bounds_y = bounds_y - bounds_height * bounds_percent_y * 0.5f;
        bounds_height = bounds_height + bounds_height * bounds_percent_y;

        play_button_bounds_width = bounds_width;
        play_button_bounds_height = bounds_height;

        nvgBeginPath(vg);
        nvgRoundedRect(vg, bounds_x, bounds_y, bounds_width, bounds_height, font_size * 0.1f);
        nvgStrokeWidth(vg, font_size * 0.25f);
        nvgStrokePaint(vg,
          nvgLinearGradient(
            vg,
            bounds_x,
            bounds_y,
            bounds_x + bounds_width,
            bounds_y + bounds_height,
            nvgRGB(colors[0].r, colors[0].g, colors[0].b),
            nvgRGB(colors[5].r, colors[5].g, colors[5].b)
          )
        );
        nvgStroke(vg);

        if(point_in_rect(mouse.x, mouse.y, bounds_x, bounds_y, bounds_width, bounds_height))
        {
          nvgFillPaint(vg,
            nvgLinearGradient(
              vg,
              bounds_x,
              bounds_y,
              bounds_x + bounds_width,
              bounds_y + bounds_height,
              nvgRGB(colors[0].r, colors[0].g, colors[0].b),
              nvgRGB(colors[5].r, colors[5].g, colors[5].b)
            )
          );
          nvgFill(vg);

          nvgFillColor(vg, nvgRGB(255, 255, 255));
          nvgText(vg, x, y, "Play", NULL);

          if(mouse_button_down)
          {
            gamestate = PLAYING;
            //Only play higher notes starting with C_high.
            Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
          }
        }
        else
        {
          nvgFillColor(vg, nvgRGB(0, 0, 0));
          nvgText(vg, x, y, "Play", NULL);
        }
      }

      //Exit Button.
      {
        float font_size = width * 0.05f;//0.025f;
        float test_height = height * 0.1f;
        if(test_height < font_size)
        {
          font_size = test_height;
        }
        float exit_button_x = play_button_x;
        float exit_button_y = play_button_y + play_button_bounds_height * GOLDEN_RATIO;
        float x = exit_button_x;
        float y = exit_button_y;
        nvgFontSize(vg, font_size);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);

        float bounds[4];
        //Use play to find the text bounds to keep the button the same size
        //as the play button.
        nvgTextBounds(vg, x, y, "Play", NULL, bounds);
        float bounds_x = bounds[0];
        float bounds_y = bounds[1];
        float bounds_width = bounds[2] - bounds[0];
        float bounds_height = bounds[3] - bounds[1];

        float bounds_percent_x = 1.0f;
        bounds_x = bounds_x - bounds_width * bounds_percent_x * 0.5f;
        bounds_width = bounds_width + bounds_width * bounds_percent_x;

        float bounds_percent_y = 0.5625f;
        bounds_y = bounds_y - bounds_height * bounds_percent_y * 0.5f;
        bounds_height = bounds_height + bounds_height * bounds_percent_y;

        nvgBeginPath(vg);
        nvgRoundedRect(vg, bounds_x, bounds_y, bounds_width, bounds_height, font_size * 0.1f);
        nvgStrokeWidth(vg, font_size * 0.25f);
        /*nvgStrokePaint(vg,
          nvgLinearGradient(
            vg,
            bounds_x,
            bounds_y,
            bounds_x + bounds_width,
            bounds_y + bounds_height,
            nvgRGB(colors[0].r, colors[0].g, colors[0].b),
            nvgRGB(colors[5].r, colors[5].g, colors[5].b)
          )
        );*/
        nvgStrokeColor(vg, nvgRGB(0, 0, 0));
        nvgStroke(vg);

        if(point_in_rect(mouse.x, mouse.y, bounds_x, bounds_y, bounds_width, bounds_height))
        {
          /*nvgFillPaint(vg,
            nvgLinearGradient(
              vg,
              bounds_x,
              bounds_y,
              bounds_x + bounds_width,
              bounds_y + bounds_height,
              nvgRGB(colors[0].r, colors[0].g, colors[0].b),
              nvgRGB(colors[5].r, colors[5].g, colors[5].b)
            )
          );*/
          nvgFillColor(vg, nvgRGB(0, 0, 0));
          nvgFill(vg);

          nvgFillColor(vg, nvgRGB(255, 255, 255));
          nvgText(vg, x, y, "Exit", NULL);

          if(mouse_button_down)
          {
            game_is_running = false;
          }
        }
        else
        {
          nvgFillColor(vg, nvgRGB(0, 0, 0));
          nvgText(vg, x, y, "Exit", NULL);
        }
      }

      if(escape_pressed)
      {
        game_is_running = false;
      }


      nvgEndFrame(vg);
    }
    else if(gamestate == PLAYING)
    {
      if(matching(games[current_game]->left_state, games[current_game]->right_state, games[current_game]->number_of_states))
      {
        //If we just won, choose a random win message.
        if(won_game == false)
        {
          current_win_message = win_messages[rand()%MAX_WIN_MESSAGES];
        }

        won_game = true;
      }
      else
      {
        won_game = false;
      }

      nvgBeginFrame(vg, width, height, 1);

      float percent = 0.61803398875f;
      float x = 0;
      //float y = (height - height * 0.1) * (1-percent) / 2.0f;
      float y = height * (1 - percent) / 2.0f;
      float w = width;
      //float h = (height - height * 0.1) * percent;
      float h = height * percent;
  /*
      nvgBeginPath(vg);
      nvgRect(vg, x, y, w, h);
      nvgClosePath(vg);
      nvgFillColor(vg, nvgRGB(0, 0, 0));
      nvgFill(vg);

      nvgBeginPath(vg);
      nvgRect(vg, width/2 - 10, height/2 - 10, 20, 20);
      nvgClosePath(vg);
      nvgFillColor(vg, nvgRGB(0, 255, 0));
      nvgFill(vg);
  */
      if(won_game)
      {
        float font_size = y * INVERSE_GOLDEN_RATIO;
        //if(width < height)
        //{
        //  font_size = width * 0.5f * INVERSE_GOLDEN_RATIO;
        //}
        nvgFontSize(vg, font_size);
        nvgFontFace(vg, "sans");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgText(vg, width/2.0f, y/2.0f, current_win_message, NULL);
      }
      //games[current_game]->draw(vg, games[current_game], 0, height/4, width, height/2, colors, mouse, mouse_button_down);
      bool collision_game = false;
      games[current_game]->draw(vg, games[current_game], x, y, w, h, colors, mouse, mouse_button_down, &collision_game);

      //Play note sfx if we clicked on a polygon in the game.
      if(collision_game)
      {
        //Only play lower notes up to C_high
        Mix_PlayChannel(-1, notes[rand()%8], 0);
      }

      nvgLineJoin(vg, NVG_MITER);
      nvgLineCap(vg, NVG_BUTT);

      float percent_toolbar = 0.18f;//0.12f;
      {
        int button_count = 6;
        float xx = 0;
        //float yy = height - (height * percent_toolbar);
        float yy = height * (1 - percent_toolbar);
        float ww = width;
        float hh = height * percent_toolbar;

        SDL_Rect collision = {xx, yy, ww, hh};
        if(won_game || SDL_PointInRect(&mouse, &collision))
        {

          /*
          nvgBeginPath(vg);
          nvgMoveTo(vg, xx, yy);
          nvgLineTo(vg, ww, yy);
          nvgClosePath(vg);

          nvgStrokeColor(vg, nvgRGB(0, 0, 0));
          nvgStrokeWidth(vg, 1);
          nvgStroke(vg);
          */



          float _w = hh * 0.61f;//0.75f;//0.86f;
          float _h = _w;
          float spacing = _w / GOLDEN_RATIO;//_w / 2.0f;
          float _x = width / 2.0f * 0.61f;
          float _y = height - hh + (hh - _h) / 2.0f;

          if(h < w)
          {
            float available_width = h * GOLDEN_RATIO;
            if(available_width > width) available_width = width;
            _x = (width - available_width) / 2.0f;
            _w = (available_width - (5.0f * spacing)) / 6.0f;
            _h = _w;
          }
          else
          {
            _x = 0;//width/2.0f - w/2.0f;
            float available_width = width;
            spacing = 1;
            _w = (available_width - (5.0f * spacing)) / 6.0f;
            _h = _w;
          }


          //Randomize State Button.
          NVGcolor color_black = nvgRGB(0, 0, 0);
          NVGcolor color_white = nvgRGB(255, 255, 255);
          NVGcolor * dice_bg_color = &color_white;
          NVGcolor * dice_fg_color = &color_black;
          if(point_in_rect(mouse.x, mouse.y, _x - (spacing / 2.0f), _y, _w + spacing, _w))
          {
            dice_bg_color = &color_black;
            dice_fg_color = &color_white;
            if(mouse_button_down)
            {
              randomize(games[current_game]);
              //Only play higher notes starting with C_high.
              Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
              int old_face = randomize_state_die_face;
              do
              {
                randomize_state_die_face = rand() % 6 + 1;
              } while(old_face == randomize_state_die_face);
            }
          }

          float radius_percent = 0.11f;//0.125f;
          float radius = radius_percent * _w;
          float stroke_width = radius * 0.61f;
          {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, _x, _y, _w, _w, radius);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_fg_color);
            nvgFill(vg);
            nvgStrokeColor(vg, *dice_bg_color);
            nvgStrokeWidth(vg, radius * 0.61f);
            nvgStroke(vg);

            draw_die_face(vg, _x, _y, _w, _h, radius, randomize_state_die_face, dice_bg_color);
          }

          //Randomize Color Button.
          _x = _x + _w + spacing;
          dice_bg_color = &color_white;
          dice_fg_color = &color_black;
          if(point_in_rect(mouse.x, mouse.y, _x - (spacing / 2.0f), _y, _w + spacing, _w))
          {
            dice_bg_color = &color_black;
            dice_fg_color = &color_white;
            if(mouse_button_down)
            {
              randomize_colors(colors, MAX_COLORS);
              //Only play higher notes starting with C_high.
              Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
              int old_face = randomize_color_die_face;
              do
              {
                randomize_color_die_face = rand() % 6 + 1;
              } while(old_face == randomize_color_die_face);
            }
          }

          {
            /*float radius_percent = 0.125f;
            float radius = radius_percent * _w;
            float cx = _x + _w / 2.0f;
            float cy = _y + _h / 2.0f;*/



            int number_of_colors = games[current_game]->mod;
            float height_of_color = _w / (float) number_of_colors;
            int last_color = number_of_colors - 1;
            for(int i = 0; i < number_of_colors; i++)
            {
              if(i == 0 || i == last_color)
              {
                nvgBeginPath(vg);
                nvgRoundedRect(vg, _x, _y + height_of_color * i, _w, height_of_color, radius);
                nvgClosePath(vg);
                nvgFillColor(vg, nvgRGB(colors[i].r, colors[i].g, colors[i].b));
                nvgFill(vg);
                nvgStrokeColor(vg, nvgRGB(colors[i].r, colors[i].g, colors[i].b));
                nvgStrokeWidth(vg, 1);
                nvgStroke(vg);

                if(i == 0)
                {
                  nvgBeginPath(vg);
                  nvgRect(vg, _x, _y + (height_of_color * 0.5f), _w, height_of_color * 0.5f);
                  nvgClosePath(vg);
                }
                else
                {
                  nvgBeginPath(vg);
                  nvgRect(vg, _x, _y + height_of_color * i, _w, height_of_color * 0.5f);
                  nvgClosePath(vg);
                }
              }
              else
              {
                nvgBeginPath(vg);
                nvgRect(vg, _x, _y + height_of_color * i, _w, height_of_color);
                nvgClosePath(vg);
              }

              nvgFillColor(vg, nvgRGB(colors[i].r, colors[i].g, colors[i].b));
              nvgFill(vg);
              nvgStrokeColor(vg, nvgRGB(colors[i].r, colors[i].g, colors[i].b));
              nvgStrokeWidth(vg, 1);
              nvgStroke(vg);
            }

            draw_die_face(vg, _x, _y, _w, _h, radius, randomize_color_die_face, dice_bg_color);

            nvgBeginPath(vg);
            nvgRoundedRect(vg, _x, _y, _w, _w, radius);
            nvgClosePath(vg);
            nvgStrokeColor(vg, *dice_bg_color);
            nvgStrokeWidth(vg, stroke_width);
            nvgStroke(vg);
          }

          float stroke_offset = stroke_width / 2.0f;

          //Increase mod button.
          _x = _x + _w + spacing;
          dice_bg_color = &color_white;
          dice_fg_color = &color_black;
          if(point_in_rect(mouse.x, mouse.y, _x - (spacing / 2.0f), _y, _w + spacing, _w / 2.0f))
          {
            dice_bg_color = &color_black;
            dice_fg_color = &color_white;
            if(mouse_button_down)
            {
              if(games[current_game]->mod < 9)
              {
                games[current_game]->mod++;
                if(games[current_game]->mod > 9)
                {
                  games[current_game]->mod = 9;
                }
                randomize(games[current_game]);
                //Only play higher notes starting with C_high.
                Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
              }
            }
          }

          {
            nvgBeginPath(vg);
            nvgRect(vg, _x, _y, _w, _w / 2.0f + stroke_offset);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_bg_color);
            nvgFill(vg);
            nvgStrokeColor(vg, *dice_bg_color);
            nvgStrokeWidth(vg, stroke_width);
            nvgStroke(vg);

            nvgBeginPath(vg);

            nvgMoveTo(vg, _x + stroke_offset, _y + (_w / 2.0f) - stroke_offset);
            nvgLineTo(vg, _x + _w - stroke_offset, _y + (_w / 2.0f) - stroke_offset);
            nvgLineTo(vg, _x + _w / 2.0f, _y + stroke_offset);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_fg_color);
            nvgFill(vg);
          }

          //Decrease mod button.
          NVGcolor color_clear = nvgRGBA(255, 255, 255, 0);
          dice_bg_color = &color_clear;
          dice_fg_color = &color_black;
          if(point_in_rect(mouse.x, mouse.y, _x - (spacing / 2.0f), _y + _w / 2.0f, _w + spacing, _w / 2.0f))
          {
            dice_bg_color = &color_black;
            dice_fg_color = &color_white;
            if(mouse_button_down)
            {
              if(games[current_game]->mod > 2)
              {
                games[current_game]->mod--;
                if(games[current_game]->mod < 2)
                {
                  games[current_game]->mod = 2;
                }
                randomize(games[current_game]);
                //Only play higher notes starting with C_high.
                Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
              }
            }
          }

          {
            nvgBeginPath(vg);
            nvgRect(vg, _x, _y + _w / 2.0f, _w, _w / 2.0f);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_bg_color);
            nvgFill(vg);
            nvgStrokeColor(vg, *dice_bg_color);
            nvgStrokeWidth(vg, stroke_width);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgMoveTo(vg, _x + stroke_offset, _y + (_w / 2.0f) + stroke_offset);
            nvgLineTo(vg, _x + _w - stroke_offset, _y + (_w / 2.0f) + stroke_offset);
            nvgLineTo(vg, _x + _w / 2.0f, _y + _w - stroke_offset);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_fg_color);
            nvgFill(vg);
          }


          //Previous Level Button.
          _x = _x + _w + spacing;
          dice_bg_color = &color_white;
          dice_fg_color = &color_black;
          if(point_in_rect(mouse.x, mouse.y, _x - (spacing / 2.0f), _y, _w + spacing, _w))
          {
            dice_bg_color = &color_black;
            dice_fg_color = &color_white;
            if(mouse_button_down)
            {
              //Cycle games backward.
              current_game--;
              if(current_game < 0) current_game = GAME_COUNT - 1;
              //Only play higher notes starting with C_high.
              Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
            }
          }

          {
            nvgBeginPath(vg);
            nvgRect(vg, _x, _y, _w, _w);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_bg_color);
            nvgFill(vg);
            nvgStrokeColor(vg, *dice_bg_color);
            nvgStrokeWidth(vg, stroke_width);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgMoveTo(vg, _x + _w - stroke_offset, _y + stroke_offset);
            nvgLineTo(vg, _x + stroke_offset, _y + stroke_offset + _w / 2.0f);
            nvgLineTo(vg, _x + _w - stroke_offset, _y + _w - stroke_offset);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_fg_color);
            nvgFill(vg);
          }

          //Next Level Button.
          _x = _x + _w + spacing;
          dice_bg_color = &color_white;
          dice_fg_color = &color_black;
          if(point_in_rect(mouse.x, mouse.y, _x - (spacing / 2.0f), _y, _w + spacing, _w))
          {
            dice_bg_color = &color_black;
            dice_fg_color = &color_white;
            if(mouse_button_down)
            {
              //Cycle games forward.
              current_game++;
              if(current_game >= GAME_COUNT) current_game = 0;
              //Only play higher notes starting with C_high.
              Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
            }
          }

          {
            nvgBeginPath(vg);
            nvgRect(vg, _x, _y, _w, _w);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_bg_color);
            nvgFill(vg);
            nvgStrokeColor(vg, *dice_bg_color);
            nvgStrokeWidth(vg, stroke_width);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgMoveTo(vg, _x + stroke_offset, _y + stroke_offset);
            nvgLineTo(vg, _x + _w - stroke_offset, _y + stroke_offset + _w / 2.0f);
            nvgLineTo(vg, _x + stroke_offset, _y + _w - stroke_offset);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_fg_color);
            nvgFill(vg);
          }

          //Main Menu Button
          _x = _x + _w + spacing;
          dice_bg_color = &color_white;
          dice_fg_color = &color_black;
          if(point_in_rect(mouse.x, mouse.y, _x - (spacing / 2.0f), _y, _w + spacing, _w))
          {
            dice_bg_color = &color_black;
            dice_fg_color = &color_white;
            if(mouse_button_down)
            {
              gamestate = MAIN_MENU;
              //Only play higher notes starting with C_high.
              Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
            }
          }

          {
            nvgBeginPath(vg);
            nvgRect(vg, _x, _y, _w, _w);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_bg_color);
            nvgFill(vg);
            nvgStrokeColor(vg, *dice_bg_color);
            nvgStrokeWidth(vg, stroke_width);
            nvgStroke(vg);

            nvgBeginPath(vg);
            nvgMoveTo(vg, _x + stroke_offset, _y + _w - stroke_offset);
            nvgLineTo(vg, _x + _w - stroke_offset, _y + _w - stroke_offset);
            nvgLineTo(vg, _x + _w / 2.0f, _y + stroke_offset);
            nvgClosePath(vg);
            nvgFillColor(vg, *dice_fg_color);
            nvgFill(vg);
          }



          /*for(int i = 1; i < button_count; i++)
          {

            nvgBeginPath(vg);
            nvgRect(vg, _x + (_w + spacing) * i, _y, _w, _h);
            nvgClosePath(vg);
            nvgFillColor(vg, nvgRGB(255, 0, 0));
            nvgFill(vg);
          }*/
        }
      }

      if(escape_pressed)
      {
        gamestate = MAIN_MENU;
        //Only play higher notes starting with C_high.
        Mix_PlayChannel(-1, notes[rand() % 8 + 7], 0);
      }

      nvgEndFrame(vg);
    }


    SDL_GL_SwapWindow(window);

    SDL_Delay(1);
  }

  //Cleanup nanovg.
  nvgDeleteGL2(vg);

  //Cleanup loaded sound effects and music.
  //First make sure the chunk is not being played.
  //Halt all channels.
  Mix_HaltChannel(-1);
  for(int i = 0; i < MAX_NOTES; i++)
  {
    if(notes[i] != NULL)
    {
      Mix_FreeChunk(notes[i]);
      notes[i] = NULL;
    }
  }

  //Free all memory and properly shutdown SDL.
  cleanup();

  //Program completed normally.
  return EXIT_SUCCESS;
}

//Free all memory and properly shutdown SDL.
static inline void
cleanup()
{
  printf("In cleanup().\n");

  if(gl_context != NULL)
  {
    printf("Deleting gl_context.\n");
    SDL_GL_DeleteContext(gl_context);
  }

  //Destroy our renderer.
  if(renderer != NULL)
  {
    printf("Destroying renderer.\n");
    SDL_DestroyRenderer(renderer);
  }

  //Close and destroy our window.
  if(window != NULL)
  {
    printf("Destroying window.\n");
    SDL_DestroyWindow(window);
  }

  //Close SDL_mixer.
  printf("Close SDL_mixer.\n");
  Mix_CloseAudio();

  //Shutdown SDL.
  printf("Shutting down SDL.\n");
  SDL_Quit();
}

void update_triforce(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors)
{

}
void draw_triforce(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision)
{
  *collision = false;
  int * outer_state = game->right_state;
  int * inner_state = game->left_state;
  float percent = 0.05f;
  bool enable_stroke = true;
  //if(percent < 0.0f) percent = 0.01f;

  const float cos30 = 0.86602540378f;
  const float sin30 = 0.5f;

  //Note 1.5 = 1 + sin(30 degrees)
  double test_h_height = height / (((4.0f / 3.0f) + percent) * 1.5f);
  double test_h_width = width / (((4.0f / 3.0f) + percent) * (2.0f * cos30));
  float h = 0.0f;
  if(width < height)
  {
    h = test_h_width;

    float hypotenuse = h * ((4.0f / 3.0f) + percent);
    float ww = 2 * cos30 * hypotenuse;
    float hh = hypotenuse * 1.5f;
    if((ww - 0.1) > width || (hh - 0.1) > height)
    {
      h = test_h_height;
    }
  }
  else
  {
    h = test_h_height;

    float hypotenuse = h * ((4.0f / 3.0f) + percent);
    float ww = 2 * cos30 * hypotenuse;
    float hh = hypotenuse * 1.5f;
    if((ww - 0.1) > width || (hh - 0.1) > height)
    {
      h = test_h_width;
    }
  }

  float half_a = h / sqrt(3);
  float a = half_a * 2;
  float s = h * percent;
  float stroke_width = s / 2.0f;

  //const float INVERSE_GOLDEN_RATIO = 0.61803398875f;

  //Data for internal triangle.
  float percent_small_triangle = INVERSE_GOLDEN_RATIO;//0.86f;
  float small_h = h * percent_small_triangle;
  float small_half_a = small_h / sqrt(3);
  float small_a = small_half_a * 2;

  static Vertex ov[4][3];
  static Vertex iv[4][3];

  //Draw top triangles.
  x = x + (width/2);
  y = y + (height/2) - ((4.0f / 3.0f * h + s) * 1.5f / 2.0f);

  ov[1][0].x = x;
  ov[1][0].y = y;
  ov[1][1].x = x - half_a;
  ov[1][1].y = y + h;
  ov[1][2].x = x + half_a;
  ov[1][2].y = y + h;

  {
    float small_x = x;
    float small_y = (y + (2.0f * h / 3.0f)) - (2.0f * small_h / 3.0f);
    iv[1][0].x = small_x;
    iv[1][0].y = small_y;
    iv[1][1].x = small_x - small_half_a;
    iv[1][1].y = small_y + small_h;
    iv[1][2].x = small_x + small_half_a;
    iv[1][2].y = small_y + small_h;
  }
/*
  //Draw outer triangle.
  nvgBeginPath(vg);
  nvgMoveTo(vg, x, y);
  nvgLineTo(vg, x - half_a, y + h);
  nvgLineTo(vg, x + half_a, y + h);
  nvgClosePath(vg);
  {
    SDL_Color color = colors[outer_state[1]];
    nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
  }
  nvgFill(vg);

  //Draw inner triangle.
  //if(draw_inner_triangles)
  {
    float small_x = x;
    float small_y = (y + (2.0f * h / 3.0f)) - (2.0f * small_h / 3.0f);

    nvgBeginPath(vg);
    nvgMoveTo(vg, small_x, small_y);
    nvgLineTo(vg, small_x - small_half_a, small_y + small_h);
    nvgLineTo(vg, small_x + small_half_a, small_y + small_h);
    nvgClosePath(vg);
    {
      SDL_Color color = colors[inner_state[1]];
      nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
    }
    nvgFill(vg);
    if(enable_stroke && !same_color(colors[outer_state[1]], colors[inner_state[1]]))
    {
      nvgStrokeColor(vg, nvgRGB(255, 255, 255));
      nvgStrokeWidth(vg, stroke_width);
      nvgStroke(vg);
    }
  }
  */

  //Draw middle triangles.
  x = x - half_a;
  y = y + h + s;

  ov[0][0].x = x;
  ov[0][0].y = y;
  ov[0][1].x = x + half_a;
  ov[0][1].y = y + h;
  ov[0][2].x = x + a;
  ov[0][2].y = y;

  {
    float small_x = x + half_a - small_half_a;
    float small_y = (y + (h / 3.0f)) - (small_h / 3.0f);
    iv[0][0].x = small_x;
    iv[0][0].y = small_y;
    iv[0][1].x = small_x + small_half_a;
    iv[0][1].y = small_y + small_h;
    iv[0][2].x = small_x + small_a;
    iv[0][2].y = small_y;
  }
/*
  //Draw outer triangle.
  nvgBeginPath(vg);
  nvgMoveTo(vg, x, y);
  nvgLineTo(vg, x + half_a, y + h);
  nvgLineTo(vg, x + a, y);
  nvgClosePath(vg);
  {
    SDL_Color color = colors[outer_state[0]];
    nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
  }
  nvgFill(vg);

  //Draw inner triangle.
  //if(draw_inner_triangles)
  {
    float small_x = x + half_a - small_half_a;
    float small_y = (y + (h / 3.0f)) - (small_h / 3.0f);

    nvgBeginPath(vg);
    nvgMoveTo(vg, small_x, small_y);
    nvgLineTo(vg, small_x + small_half_a, small_y + small_h);
    nvgLineTo(vg, small_x + small_a, small_y);
    nvgClosePath(vg);
    {
      SDL_Color color = colors[inner_state[0]];
      nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
    }
    nvgFill(vg);
    if(enable_stroke && !same_color(colors[outer_state[0]], colors[inner_state[0]]))
    {
      nvgStrokeColor(vg, nvgRGB(255, 255, 255));
      nvgStrokeWidth(vg, stroke_width);
      nvgStroke(vg);
    }

  }
*/
  //Draw left triangles.
  float third_h = 1.0f / 3.0f * h;
  float center = y + third_h;
  float hypotenuse = h + s + third_h;

  x = (x + half_a) - (cos30 * hypotenuse);
  y = (y + third_h) + (sin30 * hypotenuse);

  ov[2][0].x = x;
  ov[2][0].y = y;
  ov[2][1].x = x + a;
  ov[2][1].y = y;
  ov[2][2].x = x + half_a;
  ov[2][2].y = y - h;

  {
    float small_x = x + half_a - small_half_a;
    float small_y = (y - (h / 3.0f)) + (small_h / 3.0f);
    iv[2][0].x = small_x;
    iv[2][0].y = small_y;
    iv[2][1].x = small_x + small_a;
    iv[2][1].y = small_y;
    iv[2][2].x = small_x + small_half_a;
    iv[2][2].y = small_y - small_h;
  }

/*
  //Draw outer triangle.
  nvgBeginPath(vg);
  nvgMoveTo(vg, x, y);
  nvgLineTo(vg, x + a, y);
  nvgLineTo(vg, x + half_a, y - h);
  nvgClosePath(vg);
  {
    SDL_Color color = colors[outer_state[2]];
    nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
  }
  nvgFill(vg);

  //Draw inner triangle.
  //if(draw_inner_triangles)
  {
    float small_x = x + half_a - small_half_a;
    float small_y = (y - (h / 3.0f)) + (small_h / 3.0f);

    nvgBeginPath(vg);
    nvgMoveTo(vg, small_x, small_y);
    nvgLineTo(vg, small_x + small_a, small_y);
    nvgLineTo(vg, small_x + small_half_a, small_y - small_h);
    nvgClosePath(vg);
    {
      SDL_Color color = colors[inner_state[2]];
      nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
    }
    nvgFill(vg);
    if(enable_stroke && !same_color(colors[outer_state[2]], colors[inner_state[2]]))
    {
      nvgStrokeColor(vg, nvgRGB(255, 255, 255));
      nvgStrokeWidth(vg, stroke_width);
      nvgStroke(vg);
    }
  }
*/
  //Draw right triangles.
  x = x + 2 * cos30 * hypotenuse;

  ov[3][0].x = x;
  ov[3][0].y = y;
  ov[3][1].x = x - half_a;
  ov[3][1].y = y - h;
  ov[3][2].x = x - a;
  ov[3][2].y = y;

  {
    float small_x = x - half_a + small_half_a;
    float small_y = (y - (h / 3.0f)) + (small_h / 3.0f);
    iv[3][0].x = small_x;
    iv[3][0].y = small_y;
    iv[3][1].x = small_x - small_half_a;
    iv[3][1].y = small_y - small_h;
    iv[3][2].x = small_x - small_a;
    iv[3][2].y = small_y;
  }

/*
  //Draw outer triangle.
  nvgBeginPath(vg);
  nvgMoveTo(vg, x, y);
  nvgLineTo(vg, x - half_a, y - h);
  nvgLineTo(vg, x - a, y);
  nvgClosePath(vg);
  {
    SDL_Color color = colors[outer_state[3]];
    nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
  }
  nvgFill(vg);

  //Draw inner triangle.
  //if(draw_inner_triangles)
  {
    float small_x = x - half_a + small_half_a;
    float small_y = (y - (h / 3.0f)) + (small_h / 3.0f);

    nvgBeginPath(vg);
    nvgMoveTo(vg, small_x, small_y);
    nvgLineTo(vg, small_x - small_half_a, small_y - small_h);
    nvgLineTo(vg, small_x - small_a, small_y);
    nvgClosePath(vg);
    {
      SDL_Color color = colors[inner_state[3]];
      nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
    }
    nvgFill(vg);
    if(enable_stroke && !same_color(colors[outer_state[3]], colors[inner_state[3]]))
    {
      nvgStrokeColor(vg, nvgRGB(255, 255, 255));
      nvgStrokeWidth(vg, stroke_width);
      nvgStroke(vg);
    }
  }
*/
  if(mouse_button_down)
  {
    for(int i = 0; i < 4; i++)
    {
      if(point_in_triangle(mouse.x, mouse.y, ov[i][0].x, ov[i][0].y, ov[i][1].x, ov[i][1].y, ov[i][2].x, ov[i][2].y))
      {
        transform(
          i,
          outer_state,
          game->move_matrix,
          game->move_matrix_index,
          game->mod,
          1
        );
        *collision = true;
      }
    }
  }

  for(int i = 0; i < 4; i++)
  {
    //Draw outer triangle.
    nvgBeginPath(vg);
    nvgMoveTo(vg, ov[i][0].x, ov[i][0].y);
    nvgLineTo(vg, ov[i][1].x, ov[i][1].y);
    nvgLineTo(vg, ov[i][2].x, ov[i][2].y);
    nvgClosePath(vg);
    {
      SDL_Color color = colors[outer_state[i]];
      nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
    }
    nvgFill(vg);

    //Draw inner triangle.
    //if(draw_inner_triangles)
    {
      nvgBeginPath(vg);
      nvgMoveTo(vg, iv[i][0].x, iv[i][0].y);
      nvgLineTo(vg, iv[i][1].x, iv[i][1].y);
      nvgLineTo(vg, iv[i][2].x, iv[i][2].y);
      nvgClosePath(vg);
      {
        SDL_Color color = colors[inner_state[i]];
        nvgFillColor(vg, nvgRGB(color.r, color.g, color.b));
      }
      nvgFill(vg);
      if(enable_stroke && !same_color(colors[outer_state[i]], colors[inner_state[i]]))
      {
        nvgStrokeColor(vg, nvgRGB(255, 255, 255));
        nvgStrokeWidth(vg, stroke_width);
        nvgStroke(vg);
      }
    }
  }
}

void update_foursquare(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors)
{

}
void draw_foursquare(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision)
{
  *collision = false;
  int * outer_state = game->right_state;
  int * inner_state = game->left_state;
  float spacing_percent = 0.086f;
  float max_length = 0.0f;
  if(width < height)
  {
    max_length = width;
  }
  else
  {
    max_length = height;
  }
  //Let a be the length of one square.
  //max_length = 2 * a + (a * p) where p = spacing_percent.
  //max_length = a * (2 + p).
  //a = max_length / (2 + p).

  float side_length = max_length / (2.0f + spacing_percent);
  float rounded_length = side_length * 0.1f;
  float spacing_length = side_length * spacing_percent;

  float original_x = x;

  float percent_small_square = 0.78f;
  float small_side_length = side_length * percent_small_square;
  float small_rounded_length = small_side_length * 0.1f;
  float offset = (side_length - small_side_length) / 2.0f;

  float stroke_width = spacing_length / 2.0f;

  if(width < height)
  {
    y = y + height / 2.0f - (side_length + spacing_length / 2.0f);
  }
  else
  {
    x = width / 2.0f - (side_length + spacing_length / 2.0f);
  }

  static float xs[4];
  static float ys[4];

  //Top left.
  xs[0] = x;
  ys[0] = y;

  //Top right.
  xs[1] = x + side_length + spacing_length;
  ys[1] = y;

  //Bottom right.
  xs[2] = xs[1];
  ys[2] = y + side_length + spacing_length;

  //Bottom left.
  xs[3] = x;
  ys[3] = ys[2];

  if(mouse_button_down)
  {
    for(int i = 0; i < 4; i++)
    {
      if(point_in_square(mouse.x, mouse.y, xs[i], ys[i], side_length))
      {
        transform(
          i,
          outer_state,
          game->move_matrix,
          game->move_matrix_index,
          game->mod,
          1
        );
        *collision = true;
      }
    }
  }

  for(int i = 0; i < 4; i++)
  {
    nvgBeginPath(vg);
    nvgRoundedRect(vg, xs[i], ys[i], side_length, side_length, rounded_length);
    nvgClosePath(vg);
    SDL_Color outer_color = colors[outer_state[i]];
    nvgFillColor(vg, nvgRGB(outer_color.r, outer_color.g, outer_color.b));
    nvgFill(vg);

    SDL_Color inner_color = colors[inner_state[i]];
    if(!same_color(outer_color, inner_color))
    {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, xs[i]+offset, ys[i]+offset, small_side_length, small_side_length, small_rounded_length);
      nvgClosePath(vg);
      nvgFillColor(vg, nvgRGB(inner_color.r, inner_color.g, inner_color.b));
      nvgFill(vg);

      nvgStrokeColor(vg, nvgRGB(255, 255, 255));
      nvgStrokeWidth(vg, stroke_width);
      nvgStroke(vg);
    }
  }

}

void update_squarediamond(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors)
{

}
void draw_squarediamond(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision)
{
  *collision = false;
  int * outer_state = game->right_state;
  int * inner_state = game->left_state;
  static int indices[] = {
    0, //0
    1, //1
    4, //2
    7, //3
    10, //4
    13, //5
    14, //6
    17, //7
    20, //8
    21, //9
    22, //10
    25, //11
    28, //12
    31, //13
    34, //14
    35, //15
    36, //16
    39, //17
    42, //18
    43, //19
    46, //20
    49, //21
    52, //22
    55  //23
  };
  static Vertex outer_vertices[] = {
    {0, 0},                 //0
    {0, 0}, {0, 0}, {0, 0}, //1
    {0, 0}, {0, 0}, {0, 0}, //2
    {0, 0}, {0, 0}, {0, 0}, //3
    {0, 0}, {0, 0}, {0, 0}, //4
    {0, 0},                 //5
    {0, 0}, {0, 0}, {0, 0}, //6
    {0, 0}, {0, 0}, {0, 0}, //7
    {0, 0},                 //8
    {0, 0},                 //9
    {0, 0}, {0, 0}, {0, 0}, //10
    {0, 0}, {0, 0}, {0, 0}, //11
    {0, 0}, {0, 0}, {0, 0}, //12
    {0, 0}, {0, 0}, {0, 0}, //13
    {0, 0},                 //14
    {0, 0},                 //15
    {0, 0}, {0, 0}, {0, 0}, //16
    {0, 0}, {0, 0}, {0, 0}, //17
    {0, 0},                 //18
    {0, 0}, {0, 0}, {0, 0}, //19
    {0, 0}, {0, 0}, {0, 0}, //20
    {0, 0}, {0, 0}, {0, 0}, //21
    {0, 0}, {0, 0}, {0, 0}, //22
    {0, 0}                  //23
  };
  static Vertex inner_vertices[] = {
    {0, 0},                 //0
    {0, 0}, {0, 0}, {0, 0}, //1
    {0, 0}, {0, 0}, {0, 0}, //2
    {0, 0}, {0, 0}, {0, 0}, //3
    {0, 0}, {0, 0}, {0, 0}, //4
    {0, 0},                 //5
    {0, 0}, {0, 0}, {0, 0}, //6
    {0, 0}, {0, 0}, {0, 0}, //7
    {0, 0},                 //8
    {0, 0},                 //9
    {0, 0}, {0, 0}, {0, 0}, //10
    {0, 0}, {0, 0}, {0, 0}, //11
    {0, 0}, {0, 0}, {0, 0}, //12
    {0, 0}, {0, 0}, {0, 0}, //13
    {0, 0},                 //14
    {0, 0},                 //15
    {0, 0}, {0, 0}, {0, 0}, //16
    {0, 0}, {0, 0}, {0, 0}, //17
    {0, 0},                 //18
    {0, 0}, {0, 0}, {0, 0}, //19
    {0, 0}, {0, 0}, {0, 0}, //20
    {0, 0}, {0, 0}, {0, 0}, //21
    {0, 0}, {0, 0}, {0, 0}, //22
    {0, 0}                  //23
  };
  static int lengths[] = {
    1, //0
    3, //1
    3, //2
    3, //3
    3, //4
    1, //5
    3, //6
    3, //7
    1, //8
    1, //9
    3, //10
    3, //11
    3, //12
    3, //13
    1, //14
    1, //15
    3, //16
    3, //17
    1, //18
    3, //19
    3, //20
    3, //21
    3, //22
    1  //23
  };
  float side_length = 100.0f;
  if(width < height)
  {
    side_length = width / 4.0f;
    y = y+ (height / 2.0f - (side_length*2));
  }
  else
  {
    side_length = height / 4.0f;
    x = width / 2.0f - (side_length*2);
  }
  float small_percent = 0.75f;//0.61803398875f;
  float small_side_length = side_length * small_percent;
  float offset = (side_length - small_side_length) / 2.0f;

  //For internal triangles.
  float percent = 0.5f;
  float big_triangle_height = sqrt(0.5) * side_length;
  float small_triangle_height = big_triangle_height * percent;
  float triangle_offset_height = (big_triangle_height - small_triangle_height) / 2.0f;
  float triangle_offset = triangle_offset_height / sqrt(2);
  const float sincos45 = 0.70710678118f - 0.08f;
  float triangle_length = small_triangle_height / sincos45;



  float xs[5];
  float ys[5];

  for(int i = 0; i < 5; i++)
  {
    xs[i] = x + side_length * i;
    ys[i] = y + side_length * i;
  }

  Vertex * ov = outer_vertices;
  Vertex * iv = inner_vertices;

  //0
  ov[0].x = xs[0];
  ov[0].y = ys[0];

  iv[0].x = ov[0].x + offset;
  iv[0].y = ov[0].y + offset;

  //1
  ov[1].x = xs[1];
  ov[1].y = ys[0];
  ov[2].x = xs[1];
  ov[2].y = ys[1];
  ov[3].x = xs[2];
  ov[3].y = ys[0];

  iv[1].x = ov[1].x + triangle_offset;
  iv[1].y = ov[1].y + triangle_offset;
  iv[2].x = iv[1].x;
  iv[2].y = iv[1].y + triangle_length;
  iv[3].x = iv[1].x + triangle_length;
  iv[3].y = iv[1].y;

  //2
  ov[4].x = xs[1];
  ov[4].y = ys[1];
  ov[5].x = xs[2];
  ov[5].y = ys[1];
  ov[6].x = xs[2];
  ov[6].y = ys[0];

  iv[5].x = ov[5].x - triangle_offset;
  iv[5].y = ov[5].y - triangle_offset;
  iv[4].x = iv[5].x - triangle_length;
  iv[4].y = iv[5].y;
  iv[6].x = iv[5].x;
  iv[6].y = iv[5].y - triangle_length;

  //3
  ov[7].x = xs[2];
  ov[7].y = ys[0];
  ov[8].x = xs[2];
  ov[8].y = ys[1];
  ov[9].x = xs[3];
  ov[9].y = ys[1];

  iv[8].x = ov[8].x + triangle_offset;
  iv[8].y = ov[8].y - triangle_offset;
  iv[7].x = iv[8].x;
  iv[7].y = iv[8].y - triangle_length;
  iv[9].x = iv[8].x + triangle_length;
  iv[9].y = iv[8].y;

  //4
  ov[10].x = xs[2];
  ov[10].y = ys[0];
  ov[11].x = xs[3];
  ov[11].y = ys[1];
  ov[12].x = xs[3];
  ov[12].y = ys[0];

  iv[12].x = ov[12].x - triangle_offset;
  iv[12].y = ov[12].y + triangle_offset;
  iv[10].x = iv[12].x - triangle_length;
  iv[10].y = iv[12].y;
  iv[11].x = iv[12].x;
  iv[11].y = iv[12].y + triangle_length;

  //5
  ov[13].x = xs[3];
  ov[13].y = ys[0];

  iv[13].x = ov[13].x + offset;
  iv[13].y = ov[13].y + offset;

  //6
  ov[14].x = xs[3];
  ov[14].y = ys[1];
  ov[15].x = xs[4];
  ov[15].y = ys[2];
  ov[16].x = xs[4];
  ov[16].y = ys[1];

  iv[16].x = ov[16].x - triangle_offset;
  iv[16].y = ov[16].y + triangle_offset;
  iv[14].x = iv[16].x - triangle_length;
  iv[14].y = iv[16].y;
  iv[15].x = iv[16].x;
  iv[15].y = iv[16].y + triangle_length;

  //7
  ov[17].x = xs[4];
  ov[17].y = ys[2];
  ov[18].x = xs[3];
  ov[18].y = ys[1];
  ov[19].x = xs[3];
  ov[19].y = ys[2];

  iv[19].x = ov[19].x + triangle_offset;
  iv[19].y = ov[19].y - triangle_offset;
  iv[18].x = iv[19].x;
  iv[18].y = iv[19].y - triangle_length;
  iv[17].x = iv[19].x + triangle_length;
  iv[17].y = iv[19].y;

  //8
  ov[20].x = xs[2];
  ov[20].y = ys[1];

  iv[20].x = ov[20].x + offset;
  iv[20].y = ov[20].y + offset;

  //9
  ov[21].x = xs[1];
  ov[21].y = ys[1];

  iv[21].x = ov[21].x + offset;
  iv[21].y = ov[21].y + offset;

  //10
  ov[22].x = xs[1];
  ov[22].y = ys[1];
  ov[23].x = xs[0];
  ov[23].y = ys[2];
  ov[24].x = xs[1];
  ov[24].y = ys[2];

  iv[24].x = ov[24].x - triangle_offset;
  iv[24].y = ov[24].y - triangle_offset;
  iv[23].x = iv[24].x - triangle_length;
  iv[23].y = iv[24].y;
  iv[22].x = iv[24].x;
  iv[22].y = iv[24].y - triangle_length;

  //11
  ov[25].x = xs[0];
  ov[25].y = ys[2];
  ov[26].x = xs[1];
  ov[26].y = ys[1];
  ov[27].x = xs[0];
  ov[27].y = ys[1];

  iv[27].x = ov[27].x + triangle_offset;
  iv[27].y = ov[27].y + triangle_offset;
  iv[25].x = iv[27].x;
  iv[25].y = iv[27].y + triangle_length;
  iv[26].x = iv[27].x + triangle_length;
  iv[26].y = iv[27].y;

  //12
  ov[28].x = xs[0];
  ov[28].y = ys[2];
  ov[29].x = xs[0];
  ov[29].y = ys[3];
  ov[30].x = xs[1];
  ov[30].y = ys[3];

  iv[29].x = ov[29].x + triangle_offset;
  iv[29].y = ov[29].y - triangle_offset;
  iv[28].x = iv[29].x;
  iv[28].y = iv[29].y - triangle_length;
  iv[30].x = iv[29].x + triangle_length;
  iv[30].y = iv[29].y;

  //13
  ov[31].x = xs[0];
  ov[31].y = ys[2];
  ov[32].x = xs[1];
  ov[32].y = ys[3];
  ov[33].x = xs[1];
  ov[33].y = ys[2];

  iv[33].x = ov[33].x - triangle_offset;
  iv[33].y = ov[33].y + triangle_offset;
  iv[31].x = iv[33].x - triangle_length;
  iv[31].y = iv[33].y;
  iv[32].x = iv[33].x;
  iv[32].y = iv[33].y + triangle_length;

  //14
  ov[34].x = xs[1];
  ov[34].y = ys[2];

  iv[34].x = ov[34].x + offset;
  iv[34].y = ov[34].y + offset;


  //15
  ov[35].x = xs[2];
  ov[35].y = ys[2];

  iv[35].x = ov[35].x + offset;
  iv[35].y = ov[35].y + offset;

  //16
  ov[36].x = xs[3];
  ov[36].y = ys[2];
  ov[37].x = xs[3];
  ov[37].y = ys[3];
  ov[38].x = xs[4];
  ov[38].y = ys[2];

  iv[36].x = ov[36].x + triangle_offset;
  iv[36].y = ov[36].y + triangle_offset;
  iv[37].x = iv[36].x;
  iv[37].y = iv[36].y + triangle_length;
  iv[38].x = iv[36].x + triangle_length;
  iv[38].y = iv[36].y;

  //17
  ov[39].x = xs[3];
  ov[39].y = ys[3];
  ov[40].x = xs[4];
  ov[40].y = ys[3];
  ov[41].x = xs[4];
  ov[41].y = ys[2];

  iv[40].x = ov[40].x - triangle_offset;
  iv[40].y = ov[40].y - triangle_offset;
  iv[39].x = iv[40].x - triangle_length;
  iv[39].y = iv[40].y;
  iv[41].x = iv[40].x;
  iv[41].y = iv[40].y - triangle_length;

  //18
  ov[42].x = xs[3];
  ov[42].y = ys[3];

  iv[42].x = ov[42].x + offset;
  iv[42].y = ov[42].y + offset;

  //19
  ov[43].x = xs[3];
  ov[43].y = ys[3];
  ov[44].x = xs[2];
  ov[44].y = ys[4];
  ov[45].x = xs[3];
  ov[45].y = ys[4];

  iv[45].x = ov[45].x - triangle_offset;
  iv[45].y = ov[45].y - triangle_offset;
  iv[44].x = iv[45].x - triangle_length;
  iv[44].y = iv[45].y;
  iv[43].x = iv[45].x;
  iv[43].y = iv[45].y - triangle_length;

  //20
  ov[46].x = xs[2];
  ov[46].y = ys[4];
  ov[47].x = xs[3];
  ov[47].y = ys[3];
  ov[48].x = xs[2];
  ov[48].y = ys[3];

  iv[48].x = ov[48].x + triangle_offset;
  iv[48].y = ov[48].y + triangle_offset;
  iv[46].x = iv[48].x;
  iv[46].y = iv[48].y + triangle_length;
  iv[47].x = iv[48].x + triangle_length;
  iv[47].y = iv[48].y;

  //21
  ov[49].x = xs[2];
  ov[49].y = ys[4];
  ov[50].x = xs[2];
  ov[50].y = ys[3];
  ov[51].x = xs[1];
  ov[51].y = ys[3];

  iv[50].x = ov[50].x - triangle_offset;
  iv[50].y = ov[50].y + triangle_offset;
  iv[51].x = iv[50].x - triangle_length;
  iv[51].y = iv[50].y;
  iv[49].x = iv[50].x;
  iv[49].y = iv[50].y + triangle_length;

  //22
  ov[52].x = xs[2];
  ov[52].y = ys[4];
  ov[53].x = xs[1];
  ov[53].y = ys[3];
  ov[54].x = xs[1];
  ov[54].y = ys[4];

  iv[54].x = ov[54].x + triangle_offset;
  iv[54].y = ov[54].y - triangle_offset;
  iv[53].x = iv[54].x;
  iv[53].y = iv[54].y - triangle_length;
  iv[52].x = iv[54].x + triangle_length;
  iv[52].y = iv[54].y;

  //23
  ov[55].x = xs[0];
  ov[55].y = ys[3];

  iv[55].x = ov[55].x + offset;
  iv[55].y = ov[55].y + offset;

  if(mouse_button_down)
  {
    for(int i = 0; i < 24; i++)
    {
      if(lengths[i] == 1)
      {
        Vertex * ov = &outer_vertices[indices[i]];
        if(point_in_square(mouse.x, mouse.y, ov->x, ov->y, side_length))
        {
          transform(
            i,
            outer_state,
            game->move_matrix,
            game->move_matrix_index,
            game->mod,
            1
          );
          *collision = true;
        }
      }
      else if(lengths[i] == 3)
      {
        Vertex * ov0 = &outer_vertices[indices[i]];
        Vertex * ov1 = &outer_vertices[indices[i] + 1];
        Vertex * ov2 = &outer_vertices[indices[i] + 2];

        if(point_in_triangle(mouse.x, mouse.y, ov0->x, ov0->y, ov1->x, ov1->y, ov2->x, ov2->y))
        {
          transform(
            i,
            outer_state,
            game->move_matrix,
            game->move_matrix_index,
            game->mod,
            1
          );
          *collision = true;
        }
      }
    }
  }

  float stroke_width = side_length * 0.025f;
  //float stroke_width = side_length * 0.05f;
  //NVGcolor stroke_color = nvgRGB(0, 0, 0);
  NVGcolor stroke_color = nvgRGB(255, 255, 255);

  for(int i = 0; i < 24; i++)
  {

    //nvgLineCap(vg, NVG_ROUND);
    if(lengths[i] == 1)
    {
      Vertex * ov = &outer_vertices[indices[i]];
      nvgLineJoin(vg, NVG_ROUND);
      nvgBeginPath(vg);
      nvgRect(vg, ov->x, ov->y, side_length, side_length);
      nvgClosePath(vg);
      SDL_Color outer_color = colors[outer_state[i]];
      nvgFillColor(vg, nvgRGB(outer_color.r, outer_color.g, outer_color.b));
      nvgFill(vg);
      nvgStrokeColor(vg, stroke_color);
      nvgStrokeWidth(vg, stroke_width);
      nvgStroke(vg);

      SDL_Color inner_color = colors[inner_state[i]];
      if(!same_color(inner_color, outer_color))
      {
        Vertex * iv = &inner_vertices[indices[i]];
        nvgLineJoin(vg, NVG_MITER);
        nvgBeginPath(vg);
        nvgRect(vg, iv->x, iv->y, small_side_length, small_side_length);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGB(inner_color.r, inner_color.g, inner_color.b));
        nvgFill(vg);
        nvgStrokeColor(vg, stroke_color);
        nvgStrokeWidth(vg, stroke_width/2.0f);
        nvgStroke(vg);
      }
    }
    else if(lengths[i] == 3)
    {
      Vertex * ov0 = &outer_vertices[indices[i]];
      Vertex * ov1 = &outer_vertices[indices[i] + 1];
      Vertex * ov2 = &outer_vertices[indices[i] + 2];
      nvgLineJoin(vg, NVG_ROUND);
      nvgBeginPath(vg);
      nvgMoveTo(vg, ov0->x, ov0->y);
      nvgLineTo(vg, ov1->x, ov1->y);
      nvgLineTo(vg, ov2->x, ov2->y);
      nvgClosePath(vg);
      SDL_Color outer_color = colors[outer_state[i]];
      nvgFillColor(vg, nvgRGB(outer_color.r, outer_color.g, outer_color.b));
      nvgFill(vg);
      nvgStrokeColor(vg, stroke_color);
      nvgStrokeWidth(vg, stroke_width);
      nvgStroke(vg);

      SDL_Color inner_color = colors[inner_state[i]];
      if(!same_color(inner_color, outer_color))
      {
        Vertex * iv0 = &inner_vertices[indices[i]];
        Vertex * iv1 = &inner_vertices[indices[i] + 1];
        Vertex * iv2 = &inner_vertices[indices[i] + 2];
        nvgLineJoin(vg, NVG_MITER);
        nvgBeginPath(vg);
        nvgMoveTo(vg, iv0->x, iv0->y);
        nvgLineTo(vg, iv1->x, iv1->y);
        nvgLineTo(vg, iv2->x, iv2->y);
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGB(inner_color.r, inner_color.g, inner_color.b));
        nvgFill(vg);
        nvgStrokeColor(vg, stroke_color);
        nvgStrokeWidth(vg, stroke_width/2.0f);
        nvgStroke(vg);
      }
    }
  }
}

void draw_ammann_beenker(NVGcontext * vg, Game * game, float x, float y, float width, float height, SDL_Color * colors, SDL_Point mouse, bool mouse_button_down, bool * collision)
{
  *collision = false;
  int * outer_state = game->right_state;
  int * inner_state = game->left_state;

  float a = 100.0f;
  const float SQRT2 = 1.41421356237f;
  if(width < height)
  {
    a = width/(2.0f * SQRT2 + 2.0f);
    x = width / 2.0f - (a * (1.0f + 0.5f * SQRT2));
    y = y + (height / 2.0f) - (a * (1.0f + 0.5f * SQRT2));
  }
  else
  {
    a = height/(2.0f * SQRT2 + 2.0f);
    x = width / 2.0f - (a * (1.0f + 0.5f * SQRT2));
    y = y + a * SQRT2 * 0.5f;
  }
  //a = height / (4.0f * SQRT2 + 2.0f);



  float a_sqrt2 = a * SQRT2;
  float half_a_sqrt2 = a_sqrt2 * 0.5f;

  float small_percent = 0.75f;

  float sa = a * small_percent;
  float sa_sqrt2 = sa * SQRT2;
  float half_sa_sqrt2 = sa_sqrt2 * 0.5f;

  float offset = (a - sa) / 2.0f;
  float offset_sqrt2 = (a_sqrt2 - sa_sqrt2) / 2.0f;

  //const float cos15 = 0.96592582628f;
  //const float sin15 = 0.2588190451f;
  const float cos30 = 0.86602540378f;
  const float sin30 = 0.5f;

  //All polygons have four vertices.
  static Vertex ov[96];
  static Vertex iv[96];

  for(int i = 0; i < 96; i++)
  {
    iv[i].x = 0.0f;
    iv[i].y = 0.0f;
  }

  //x = x + half_a_sqrt2;
  //y = y + half_a_sqrt2;

  //0
  ov[0].x = x;
  ov[0].y = y;
  ov[1].x = x;
  ov[1].y = y + a;
  ov[2].x = x + a;
  ov[2].y = ov[1].y;
  ov[3].x = x + a;
  ov[3].y = y;

  float sx = x + offset;
  float sy = y + offset;

  iv[0].x = sx;
  iv[0].y = sy;
  iv[1].x = sx;
  iv[1].y = sy + sa;
  iv[2].x = sx + sa;
  iv[2].y = iv[1].y;
  iv[3].x = sx + sa;
  iv[3].y = sy;

  //1
  ov[4].x = ov[3].x;
  ov[4].y = ov[3].y;
  ov[5].x = ov[4].x + half_a_sqrt2;
  ov[5].y = ov[4].y + half_a_sqrt2;
  ov[6].x = ov[4].x + a_sqrt2;
  ov[6].y = ov[4].y;
  ov[7].x = ov[5].x;
  ov[7].y = ov[5].y - a_sqrt2;

  iv[4].x = ov[4].x + offset_sqrt2;
  iv[4].y = ov[4].y;
  iv[5].x = iv[4].x + half_sa_sqrt2;
  iv[5].y = iv[4].y + half_sa_sqrt2;
  iv[6].x = iv[5].x + half_sa_sqrt2;
  iv[6].y = iv[4].y;
  iv[7].x = iv[5].x;
  iv[7].y = iv[6].y - half_sa_sqrt2;

  //2
  ov[8].x = ov[7].x;
  ov[8].y = ov[7].y;
  ov[9].x = ov[8].x - a;
  ov[9].y = ov[8].y;
  ov[10].x = ov[0].x;
  ov[10].y = ov[0].y;
  ov[11].x = ov[3].x;
  ov[11].y = ov[3].y;

  /*{
    float length = 2 * cos30 * a;
    float small_length = 2 * cos30 * sa;
    float remaining_length = (length - small_length) / 2.0f;
    float new_offset_x = remaining_length * cos30;
    float new_offset_y = remaining_length * sin30;
    iv[10].x = ov[10].x + new_offset_x;
    iv[10].y = ov[10].y - new_offset_y;
    iv[11].x = iv[10].x + sa;
    iv[11].y = iv[10].y;
    iv[8].x = ov[8].x - new_offset_x;
    iv[8].y = ov[8].y + new_offset_y;
    iv[9].x = iv[8].x - sa;
    iv[9].y = iv[8].y;
  }*/
  {
    float x8_10 = ((ov[8].x - ov[10].x) * 0.15f);
    float y8_10 = ((ov[10].y - ov[8].y) * 0.15f);
    float x11_9 = ((ov[11].x - ov[9].x) * 0.15f);
    iv[10].x = ov[10].x + x8_10;
    iv[10].y = ov[10].y - y8_10;
    iv[11].x = ov[11].x - x11_9;
    iv[11].y = iv[10].y;
    iv[8].x = ov[8].x - x8_10;
    iv[8].y = ov[8].y + y8_10;
    iv[9].x = ov[9].x + x11_9;
    iv[9].y = iv[8].y;
  }

  //3
  ov[12].x = ov[7].x;
  ov[12].y = ov[7].y;
  ov[13].x = ov[6].x;
  ov[13].y = ov[6].y;
  ov[14].x = ov[13].x + a;
  ov[14].y = ov[13].y;
  ov[15].x = ov[12].x + a;
  ov[15].y = ov[12].y;

  {
    float x_14_12 = (ov[14].x - ov[12].x) * 0.15f;
    float y_14_12 = (ov[14].y - ov[12].y) * 0.15f;
    float x_15_13 = (ov[15].x - ov[13].x) * 0.15f;
    float y_13_15 = (ov[13].y - ov[15].y) * 0.15f;

    iv[12].x = ov[12].x + x_14_12;
    iv[12].y = ov[12].y + y_14_12;
    iv[13].x = ov[13].x + x_15_13;
    iv[13].y = ov[13].y - y_13_15;
    iv[14].x = ov[14].x - x_14_12;
    iv[14].y = iv[13].y;
    iv[15].x = ov[15].x - x_15_13;
    iv[15].y = iv[12].y;
  }

  //4
  ov[16].x = ov[13].x;
  ov[16].y = ov[13].y;
  ov[17].x = ov[16].x;
  ov[17].y = ov[16].y + a;
  ov[18].x = ov[14].x;
  ov[18].y = ov[17].y;
  ov[19].x = ov[14].x;
  ov[19].y = ov[14].y;

  iv[16].x = ov[16].x + offset;
  iv[16].y = ov[16].y + offset;
  iv[17].x = iv[16].x;
  iv[17].y = iv[16].y + sa;
  iv[18].x = iv[17].x + sa;
  iv[18].y = iv[17].y;
  iv[19].x = iv[18].x;
  iv[19].y = iv[16].y;

  //5
  ov[20].x = ov[18].x;
  ov[20].y = ov[18].y;
  ov[21].x = ov[20].x - half_a_sqrt2;
  ov[21].y = ov[20].y + half_a_sqrt2;
  ov[22].x = ov[20].x;
  ov[22].y = ov[20].y + a_sqrt2;
  ov[23].x = ov[21].x + a_sqrt2;
  ov[23].y = ov[21].y;

  iv[20].x = ov[20].x;
  iv[20].y = ov[20].y + offset_sqrt2;
  iv[21].x = iv[20].x - half_sa_sqrt2;
  iv[21].y = iv[20].y + half_sa_sqrt2;
  iv[22].x = iv[20].x;
  iv[22].y = iv[20].y + sa_sqrt2;
  iv[23].x = iv[21].x + sa_sqrt2;
  iv[23].y = iv[21].y;

  //6
  ov[24].x = ov[23].x;
  ov[24].y = ov[23].y;
  ov[25].x = ov[24].x;
  ov[25].y = ov[24].y - a;
  ov[26].x = ov[19].x;
  ov[26].y = ov[19].y;
  ov[27].x = ov[18].x;
  ov[27].y = ov[18].y;

  {
    float x_24_26 = (ov[24].x - ov[26].x) * 0.15f;
    float y_24_26 = (ov[24].y - ov[26].y) * 0.15f;
    float y_27_25 = (ov[27].y - ov[25].y) * 0.15f;

    iv[24].x = ov[24].x - x_24_26;
    iv[24].y = ov[24].y - y_24_26;
    iv[25].x = iv[24].x;
    iv[25].y = ov[25].y + y_27_25;
    iv[26].x = ov[26].x + x_24_26;
    iv[26].y = ov[26].y + y_24_26;
    iv[27].x = iv[26].x;
    iv[27].y = ov[27].y - y_27_25;
  }

  //7
  ov[28].x = ov[22].x;
  ov[28].y = ov[22].y;
  ov[29].x = ov[17].x;
  ov[29].y = ov[28].y;
  ov[30].x = ov[29].x;
  ov[30].y = ov[29].y + a;
  ov[31].x = ov[28].x;
  ov[31].y = ov[30].y;

  iv[28].x = iv[18].x;
  iv[28].y = ov[28].y + offset;
  iv[29].x = iv[17].x;
  iv[29].y = iv[28].y;
  iv[30].x = iv[29].x;
  iv[30].y = iv[29].y + sa;
  iv[31].x = iv[28].x;
  iv[31].y = iv[30].y;

  //8
  ov[32].x = ov[23].x;
  ov[32].y = ov[23].y;
  ov[33].x = ov[28].x;
  ov[33].y = ov[28].y;
  ov[34].x = ov[31].x;
  ov[34].y = ov[31].y;
  ov[35].x = ov[32].x;
  ov[35].y = ov[32].y + a;

  {
    float y_34_32 = (ov[34].y - ov[32].y) * 0.15f;
    float y_35_33 = (ov[35].y - ov[33].y) * 0.15f;

    iv[32].x = iv[25].x;
    iv[32].y = ov[32].y + y_34_32;
    iv[33].x = iv[27].x;
    iv[33].y = ov[33].y + y_35_33;
    iv[34].x = iv[33].x;
    iv[34].y = ov[34].y - y_34_32;
    iv[35].x = iv[32].x;
    iv[35].y = ov[35].y - y_35_33;
  }

  //9
  ov[36].x = ov[30].x;
  ov[36].y = ov[30].y;
  ov[37].x = ov[5].x;
  ov[37].y = ov[36].y - half_a_sqrt2;
  ov[38].x = ov[4].x;
  ov[38].y = ov[36].y;
  ov[39].x = ov[37].x;
  ov[39].y = ov[37].y + a_sqrt2;

  iv[36].x = iv[6].x;
  iv[36].y = ov[36].y;
  iv[37].x = iv[7].x;
  iv[37].y = iv[36].y - half_sa_sqrt2;
  iv[38].x = iv[4].x;
  iv[38].y = iv[36].y;
  iv[39].x = iv[37].x;
  iv[39].y = iv[36].y + half_sa_sqrt2;


  //10
  ov[40].x = ov[36].x;
  ov[40].y = ov[36].y;
  ov[41].x = ov[39].x;
  ov[41].y = ov[39].y;
  ov[42].x = ov[15].x;
  ov[42].y = ov[41].y;
  ov[43].x = ov[31].x;
  ov[43].y = ov[31].y;

  {
    float y_42_40 = (ov[42].y - ov[40].y) * 0.15f;
    float y_41_43 = (ov[41].y - ov[43].y) * 0.15f;

    iv[40].x = iv[13].x;
    iv[40].y = ov[40].y + y_42_40;
    iv[41].x = iv[12].x;
    iv[41].y = ov[41].y - y_41_43;
    iv[42].x = iv[15].x;
    iv[42].y = iv[41].y;
    iv[43].x = iv[14].x;
    iv[43].y = iv[40].y;
  }

  //11
  ov[44].x = ov[38].x;
  ov[44].y = ov[38].y;
  ov[45].x = ov[2].x;
  ov[45].y = ov[29].y;
  ov[46].x = ov[0].x;
  ov[46].y = ov[45].y;
  ov[47].x = ov[46].x;
  ov[47].y = ov[44].y;

  iv[44].x = iv[2].x;
  iv[44].y = iv[30].y;
  iv[45].x = iv[44].x;
  iv[45].y = iv[28].y;
  iv[46].x = iv[1].x;
  iv[46].y = iv[45].y;
  iv[47].x = iv[46].x;
  iv[47].y = iv[44].y;

  //12
  ov[48].x = ov[39].x;
  ov[48].y = ov[39].y;;
  ov[49].x = ov[38].x;
  ov[49].y = ov[38].y;
  ov[50].x = ov[47].x;
  ov[50].y = ov[47].y;
  ov[51].x = ov[9].x;
  ov[51].y = ov[48].y;

  iv[48].x = iv[8].x;
  iv[48].y = iv[41].y;
  iv[49].x = iv[11].x;
  iv[49].y = iv[43].y;
  iv[50].x = iv[10].x;
  iv[50].y = iv[49].y;
  iv[51].x = iv[9].x;
  iv[51].y = iv[48].y;

  //13
  ov[52].x = ov[46].x;
  ov[52].y = ov[46].y;
  ov[53].x = ov[52].x + half_a_sqrt2;
  ov[53].y = ov[21].y;
  ov[54].x = ov[1].x;
  ov[54].y = ov[1].y;
  ov[55].x = ov[54].x - half_a_sqrt2;
  ov[55].y = ov[53].y;

  iv[52].x = ov[52].x;
  iv[52].y = iv[22].y;
  iv[53].x = ov[53].x - offset_sqrt2;
  iv[53].y = iv[21].y;
  iv[54].x = ov[54].x;
  iv[54].y = iv[20].y;
  iv[55].x = ov[55].x + offset_sqrt2;
  iv[55].y = iv[53].y;

  //14
  ov[56].x = ov[47].x;
  ov[56].y = ov[47].y;
  ov[57].x = ov[52].x;
  ov[57].y = ov[52].y;
  ov[58].x = ov[55].x;
  ov[58].y = ov[55].y;
  ov[59].x = ov[58].x;
  ov[59].y = ov[35].y;

  {
    float x_56_58 = (ov[56].x - ov[58].x) * 0.15f;

    iv[56].x = ov[56].x - x_56_58;
    iv[56].y = iv[34].y;
    iv[57].x = iv[56].x;
    iv[57].y = iv[33].y;
    iv[58].x = ov[58].x + x_56_58;
    iv[58].y = iv[32].y;
    iv[59].x = iv[58].x;
    iv[59].y = iv[35].y;
  }

  //15
  ov[60].x = ov[54].x;
  ov[60].y = ov[54].y;
  ov[61].x = ov[0].x;
  ov[61].y = ov[0].y;
  ov[62].x = ov[58].x;
  ov[62].y = ov[25].y;
  ov[63].x = ov[55].x;
  ov[63].y = ov[55].y;

  iv[60].x = iv[57].x;
  iv[60].y = iv[27].y;
  iv[61].x = iv[60].x;
  iv[61].y = iv[26].y;
  iv[62].x = iv[59].x;
  iv[62].y = iv[25].y;
  iv[63].x = iv[62].x;
  iv[63].y = iv[24].y;

  static Vertex center;
  center.x = ov[53].x + a;
  center.y = ov[53].y;

  //16
  ov[64].x = ov[53].x;
  ov[64].y = ov[53].y;
  ov[65].x = center.x;
  ov[65].y = center.y;
  ov[66].x = ov[2].x;
  ov[66].y = ov[2].y;
  ov[67].x = ov[60].x;
  ov[67].y = ov[60].y;

  {
    float y_64_66 = (ov[64].y - ov[66].y) * 0.15f;

    iv[64].x = iv[51].x;
    iv[64].y = ov[64].y - y_64_66;
    iv[65].x = iv[48].x;
    iv[65].y = iv[64].y;
    iv[66].x = iv[49].x;
    iv[66].y = ov[66].y + y_64_66;
    iv[67].x = iv[50].x;
    iv[67].y = iv[66].y;
  }

  //17
  ov[68].x = center.x;
  ov[68].y = center.y;
  ov[69].x = ov[5].x;
  ov[69].y = ov[5].y;
  ov[70].x = ov[3].x;
  ov[70].y = ov[3].y;
  ov[71].x = ov[2].x;
  ov[71].y = ov[2].y;

  {
    float x_68_70 = (ov[68].x - ov[70].x) * 0.15f;

    iv[68].x = ov[68].x - x_68_70;
    iv[68].y = iv[24].y;
    iv[69].x = iv[68].x;
    iv[69].y = iv[62].y;
    iv[70].x = ov[70].x + x_68_70;
    iv[70].y = iv[61].y;
    iv[71].x = iv[70].x;
    iv[71].y = iv[60].y;
  }

  //18
  ov[72].x = center.x;
  ov[72].y = center.y;
  ov[73].x = ov[17].x;
  ov[73].y = ov[17].y;
  ov[74].x = ov[6].x;
  ov[74].y = ov[6].y;
  ov[75].x = ov[69].x;
  ov[75].y = ov[69].y;

  {
    float x_74_72 = (ov[74].x - ov[72].x) * 0.15f;
    float x_73_75 = (ov[73].x - ov[75].x) * 0.15f;

    iv[72].x = ov[72].x + x_74_72;
    iv[72].y = iv[68].y;
    iv[73].x = ov[73].x - x_73_75;
    iv[73].y = iv[60].y;
    iv[74].x = iv[73].x;
    iv[74].y = iv[61].y;
    iv[75].x = iv[72].x;
    iv[75].y = iv[62].y;
  }

  //19
  ov[76].x = center.x;
  ov[76].y = center.y;
  ov[77].x = ov[21].x;
  ov[77].y = ov[21].y;
  ov[78].x = ov[20].x;
  ov[78].y = ov[20].y;
  ov[79].x = ov[73].x;
  ov[79].y = ov[73].y;

  iv[76].x = iv[41].x;
  iv[76].y = iv[65].y;
  iv[77].x = iv[42].x;
  iv[77].y = iv[76].y;
  iv[78].x = iv[43].x;
  iv[78].y = iv[67].y;
  iv[79].x = iv[40].x;
  iv[79].y = iv[78].y;

  //20
  ov[80].x = center.x;
  ov[80].y = center.y;
  ov[81].x = ov[29].x;
  ov[81].y = ov[29].y;
  ov[82].x = ov[28].x;
  ov[82].y = ov[28].y;
  ov[83].x = ov[21].x;
  ov[83].y = ov[21].y;

  {
    float y_82_80 = (ov[82].y - ov[80].y) * 0.15f;
    float y_81_83 = (ov[81].y - ov[83].y) * 0.15f;

    iv[80].x = iv[76].x;
    iv[80].y = ov[80].y + y_82_80;
    iv[81].x = iv[79].x;
    iv[81].y = ov[81].y - y_81_83;
    iv[82].x = iv[78].x;
    iv[82].y = iv[81].y;
    iv[83].x = iv[77].x;
    iv[83].y = iv[80].y;
  }

  //21
  ov[84].x = center.x;
  ov[84].y = center.y;
  ov[85].x = ov[37].x;
  ov[85].y = ov[37].y;
  ov[86].x = ov[30].x;
  ov[86].y = ov[30].y;
  ov[87].x = ov[81].x;
  ov[87].y = ov[81].y;

  iv[84].x = iv[75].x;
  iv[84].y = iv[58].y;
  iv[85].x = iv[84].x;
  iv[85].y = iv[59].y;
  iv[86].x = iv[74].x;
  iv[86].y = iv[56].y;
  iv[87].x = iv[86].x;
  iv[87].y = iv[57].y;

  //22
  ov[88].x = center.x;
  ov[88].y = center.y;
  ov[89].x = ov[45].x;
  ov[89].y = ov[45].y;
  ov[90].x = ov[44].x;
  ov[90].y = ov[44].y;
  ov[91].x = ov[85].x;
  ov[91].y = ov[85].y;

  iv[88].x = iv[68].x;
  iv[88].y = iv[84].y;
  iv[89].x = iv[71].x;
  iv[89].y = iv[33].y;
  iv[90].x = iv[89].x;
  iv[90].y = iv[34].y;
  iv[91].x = iv[88].x;
  iv[91].y = iv[35].y;

  //23
  ov[92].x = center.x;
  ov[92].y = center.y;
  ov[93].x = ov[64].x;
  ov[93].y = ov[64].y;
  ov[94].x = ov[52].x;
  ov[94].y = ov[52].y;
  ov[95].x = ov[89].x;
  ov[95].y = ov[89].y;

  iv[92].x = iv[48].x;
  iv[92].y = iv[83].y;
  iv[93].x = iv[64].x;
  iv[93].y = iv[92].y;
  iv[94].x = iv[67].x;
  iv[94].y = iv[82].y;
  iv[95].x = iv[66].x;
  iv[95].y = iv[94].y;

  if(mouse_button_down)
  {
    for(int i = 0; i < 24; i++)
    {
      int i0 = i * 4;
      int i1 = i0 + 1;
      int i2 = i1 + 1;
      int i3 = i2 + 1;
      if(point_in_quad(mouse.x, mouse.y, ov[i0].x, ov[i0].y, ov[i1].x, ov[i1].y, ov[i2].x, ov[i2].y, ov[i3].x, ov[i3].y))
      {
        transform(
          i,
          outer_state,
          game->move_matrix,
          game->move_matrix_index,
          game->mod,
          1
        );
        *collision = true;
      }
    }
  }

  float stroke_width = a * 0.025f;
  //float stroke_width = side_length * 0.05f;
  //NVGcolor stroke_color = nvgRGB(0, 0, 0);
  NVGcolor stroke_color = nvgRGB(255, 255, 255);

  for(int i = 0; i < 96; i = i + 4)
  {
    nvgLineJoin(vg, NVG_ROUND);
    nvgBeginPath(vg);
    nvgMoveTo(vg, ov[i].x, ov[i].y);
    nvgLineTo(vg, ov[i+1].x, ov[i+1].y);
    nvgLineTo(vg, ov[i+2].x, ov[i+2].y);
    nvgLineTo(vg, ov[i+3].x, ov[i+3].y);
    nvgClosePath(vg);
    SDL_Color outer_color = colors[outer_state[i/4]];
    nvgFillColor(vg, nvgRGB(outer_color.r, outer_color.g, outer_color.b));
    nvgFill(vg);
    nvgStrokeColor(vg, stroke_color);
    nvgStrokeWidth(vg, stroke_width);
    nvgStroke(vg);
  }
  for(int i = 0; i < 96; i = i + 4)
  {
    SDL_Color outer_color = colors[outer_state[i/4]];
    SDL_Color inner_color = colors[inner_state[i/4]];
    if(!same_color(inner_color, outer_color))
    {
      nvgLineJoin(vg, NVG_ROUND);
      nvgBeginPath(vg);
      nvgMoveTo(vg, iv[i].x, iv[i].y);
      nvgLineTo(vg, iv[i+1].x, iv[i+1].y);
      nvgLineTo(vg, iv[i+2].x, iv[i+2].y);
      nvgLineTo(vg, iv[i+3].x, iv[i+3].y);
      nvgClosePath(vg);
      nvgFillColor(vg, nvgRGB(inner_color.r, inner_color.g, inner_color.b));
      nvgFill(vg);
      nvgStrokeColor(vg, stroke_color);
      nvgStrokeWidth(vg, stroke_width/2.0f);
      nvgStroke(vg);
    }
  }
}

static inline bool same_color(SDL_Color c1, SDL_Color c2)
{
  return (c1.r == c2.r && c1.g == c2.g && c1.b == c2.b);
}

//Randomize colors.
static inline void randomize_colors(SDL_Color * colors, int size)
{
  for(int i = 0; i < size; i++)
  {
    colors[i].r = rand()%256;
    colors[i].g = rand()%256;
    colors[i].b = rand()%256;
  }
}

static inline bool point_in_square(float mX, float mY, float x, float y, float s)
{
  return (!(mX < x || mX > (x+s) || mY < y || mY > (y+s)));
}

static bool point_in_triangle(float x, float y, float x1, float y1, float x2, float y2, float x3, float y3)
{
  x1 -= x; x2 -= x; x3 -= x;
  y1 -= y; y2 -= y; y3 -= y;

  if(!((x2*y1-x1*y2) > 0)) return false;
  if(!((x3*y2-x2*y3) > 0)) return false;
  if(!((x1*y3-x3*y1) > 0)) return false;
  return true;
}

static bool point_in_quad(float x, float y, float p0x, float p0y, float p1x, float p1y, float p2x, float p2y, float p3x, float p3y)
{
  p0x -= x; p1x -= x; p2x -= x; p3x -= x;
  p0y -= y; p1y -= y; p2y -= y; p3y -= y;

  if(!((p1x*p0y-p0x*p1y) > 0)) return false;
  if(!((p2x*p1y-p1x*p2y) > 0)) return false;
  if(!((p3x*p2y-p2x*p3y) > 0)) return false;
  if(!((p0x*p3y-p3x*p0y) > 0)) return false;
  return true;
}

static bool point_in_rect(float mX, float mY, float x, float y, float w, float h)
{
  if(mX < x || mX > (x + w) || mY < y || mY > (y + h))
  {
    return false;
  }
  return true;
}

static void draw_die_face(NVGcontext * vg, float x, float y, float width, float height, float radius, int face, const NVGcolor * const color)
{
  float w = width;
  float h = height;
  switch(face)
  {
    case 1:
    {
      float cx = x + w / 2.0f;
      float cy = y + h / 2.0f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);
      break;
    }
    case 2:
    {
      float cx = x + radius * 2.25f;
      float cy = y + radius * 2.25f;
      //float cy = y + y / 30.0f;
      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);
      break;
    }
    case 3:
    {
      float cx = x + w / 2.0f;
      float cy = y + h / 2.0f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      break;
    }
    case 4:
    {
      float cx = x + radius * 2.25f;
      float cy = y + radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      break;
    }
    case 5:
    {
      float cx = x + radius * 2.25f;
      float cy = y + radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w / 2.0f;
      cy = y + h / 2.0f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      break;
    }
    case 6:
    {
      float cx = x + radius * 2.25f;
      float cy = y + radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + radius * 2.25f;
      cy = y + w - radius * 2.25f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + radius * 2.25f;
      cy = y + h / 2.0f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      cx = x + w - radius * 2.25f;
      cy = y + h / 2.0f;

      nvgBeginPath(vg);
      nvgCircle(vg, cx, cy, radius);
      nvgClosePath(vg);
      nvgFillColor(vg, *color);
      nvgFill(vg);

      break;
    }
  }
}
