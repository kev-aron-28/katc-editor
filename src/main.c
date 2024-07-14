#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

#define KATC_LINE_LEN 100
#define KATC_DEFAULT_LINES_LEN 100

struct katc_text_line
{
  char *text;
  int x;
  int row;
  int defined_len;
  int current_len;
};

struct katc_editor
{
  struct katc_text_line *lines;
  int row_count;
  int defined_row_len;
};

struct katc_text_line katc_default_line_value()
{
  struct katc_text_line line;
  line.text = NULL;
  line.x = 1;
  line.row = 1;
  line.defined_len = KATC_LINE_LEN;
  line.current_len = 0;
  return line;
}

struct katc_editor katc_default_editor_value()
{
  struct katc_editor editor;
  editor.lines = NULL;
  editor.row_count = 0;
  editor.defined_row_len = KATC_DEFAULT_LINES_LEN;
  return editor;
}

void katc_append_char_to_line(struct katc_text_line *line, char c, int current_row)
{
  if (line->text == NULL)
  {
    line->text = (char *)malloc(KATC_LINE_LEN * sizeof(char));
    line->current_len = 0;
    line->defined_len = KATC_LINE_LEN;
  }

  if (line->current_len >= line->defined_len - 1)
  {
    line->defined_len *= 2;
    line->text = (char *)realloc(line->text, line->defined_len * sizeof(char));
  }

  line->text[line->current_len] = c;
  line->current_len++;
  line->text[line->current_len] = '\0';
}

void katc_append_line_to_editor(struct katc_editor *editor, struct katc_text_line line)
{
  if (editor->lines == NULL)
  {
    editor->lines = (struct katc_text_line *)malloc(KATC_DEFAULT_LINES_LEN * sizeof(struct katc_text_line));
    editor->defined_row_len = KATC_DEFAULT_LINES_LEN;
  }

  if (editor->row_count >= editor->defined_row_len)
  {
    editor->defined_row_len *= 2;
    editor->lines = (struct katc_text_line *)realloc(editor->lines, editor->defined_row_len * sizeof(struct katc_text_line));
  }

  printf("LINE LEN %d \n", line.current_len);
  editor->lines[editor->row_count] = line;
  editor->row_count++;
}

struct katc_editor start_editor(char *file_to_open)
{
  struct katc_editor editor = katc_default_editor_value();

  FILE *file = fopen(file_to_open, "r");

  if (file == NULL)
  {
    printf("Error opening the file\n");
    exit(1);
  }

  char c;
  int current_row = 1;

  struct katc_text_line current_line = katc_default_line_value();
  while ((c = getc(file)) != EOF)
  {
    if (c == '\n')
    {
      katc_append_line_to_editor(&editor, current_line);
      current_line = katc_default_line_value();
      current_row++;
    }
    else
    {
      katc_append_char_to_line(&current_line, c, current_row);
    }
  }

  fclose(file);
  return editor;
}

int main(int argc, char *argv[])
{
  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  // Initialize SDL_ttf
  if (TTF_Init() == -1)
  {
    printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
    SDL_Quit();
    return 1;
  }

  // Create a window
  SDL_Window *window = SDL_CreateWindow(
      "SDL TTF Example",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      800,
      600,
      SDL_WINDOW_SHOWN);

  if (window == NULL)
  {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  // Create a renderer
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL)
  {
    printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  // Load a font
  TTF_Font *font = TTF_OpenFont("fonts/clacon2.ttf", 24);
  if (font == NULL)
  {
    printf("Failed to load font! TTF_Error: %s\n", TTF_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 1;
  }

  // Load text lines into editor
  struct katc_editor editor = start_editor("src/main.c");

  int quit = 0;
  SDL_Event e;
  SDL_Texture *textTexture;
  SDL_Surface *textSurface;

  int scrollOffset = 0;
  int lineHeight = 24; // Adjust based on your font size
  int viewportHeight;

  int cursor_row = 0;
  int cursor_col = 0;

  while (!quit)
  {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0)
    {
      if (e.type == SDL_QUIT)
      {
        quit = 1;
      }
      else if (e.type == SDL_MOUSEWHEEL)
      {
        // Adjust scroll offset with mouse wheel
        scrollOffset -= e.wheel.y * lineHeight;

        // Clamp scroll offset to valid range
        if (scrollOffset < 0)
        {
          scrollOffset = 0;
        }
        int maxScroll = editor.row_count * lineHeight - viewportHeight; // Adjust 600 based on window height
        if (scrollOffset > maxScroll)
        {
          scrollOffset = maxScroll;
        }
      } else if(e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym)
        {
        case SDLK_UP:
          if(cursor_row > 0) cursor_row--;
          break;
        case SDLK_DOWN:
          if(cursor_row < editor.row_count - 1) cursor_row++;
          break;
        case SDLK_LEFT:
          if(cursor_col > 0) cursor_col--;
          break;
        case SDLK_RIGHT:
          if(cursor_col < editor.lines[cursor_row].current_len) cursor_col++;
          break;
        default:
          break;
        }
      }
    }

    int screenWidth, screenHeight;
    SDL_GetWindowSize(window, &screenWidth, &screenHeight);
    viewportHeight = screenHeight;

    // Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Render visible lines
    int startLine = scrollOffset / lineHeight;
    int endLine = (scrollOffset + viewportHeight) / lineHeight + 1; // Adjust 600 based on window height
    int y = -scrollOffset % lineHeight;

    for (int i = startLine; i < endLine && i < editor.row_count; i++)
    { 

      if(editor.lines[i].current_len == 0) {
        textSurface = TTF_RenderText_Solid(font, " ", (SDL_Color){255, 255, 255, 255});
      } else {
        textSurface = TTF_RenderText_Solid(font, editor.lines[i].text, (SDL_Color){255, 255, 255, 255});
      }
      
      if (textSurface == NULL)
      {
        printf("Unable to render text surface! TTF_Error: %s\n", TTF_GetError());
        break;
      }

      // Create texture from surface
      textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
      if (textTexture == NULL)
      {
        printf("Unable to create texture from rendered text! SDL_Error: %s\n", SDL_GetError());
        SDL_FreeSurface(textSurface);
        break;
      }

      // Get dimensions
      int textWidth = textSurface->w;
      int textHeight = textSurface->h;
      SDL_FreeSurface(textSurface);

      // Render texture
      SDL_Rect renderQuad = {10, y, textWidth, textHeight};
      SDL_RenderCopy(renderer, textTexture, NULL, &renderQuad);

      // Free texture
      SDL_DestroyTexture(textTexture);

      if(i == cursor_row) {
        int actualX = 10;

        if(editor.lines[i].current_len == 0) {
          actualX = 10;
        } else {
          if(cursor_col > 0) {
            actualX += cursor_col * (textWidth / editor.lines[i].current_len) + 1;
          } else {
            actualX = 10;
          }

          if (cursor_col == editor.lines[i].current_len) {
            actualX += textWidth / editor.lines[i].current_len;
          }
        }
      
        SDL_Rect cursor_rect = { actualX, y, 3, textHeight };
        SDL_SetRenderDrawColor(renderer, 72, 202, 228, 0);
        SDL_RenderFillRect(renderer, &cursor_rect);
      }


      // Move to next line position
      y += lineHeight;
    }

    // Update screen
    SDL_RenderPresent(renderer);
  }

  // Cleanup
  SDL_DestroyTexture(textTexture);
  TTF_CloseFont(font);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();

  return 0;
}

// TODO: Edit file
// TODO: Show message if no file provided
