using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using System;
using System.Collections.Generic;

namespace Rocket;

public class Game1 : Game
{
    private GraphicsDeviceManager _graphics;
    private SpriteBatch _spriteBatch;
    private SpriteFont _font;
    private List<Texture2D> _tiles = [];
    private KeyboardState _keyboardState = new();
    private Rocket _rocket = new();

    public Game1()
    {
        _graphics = new GraphicsDeviceManager(this)
        {
            PreferredBackBufferWidth = 1024,
            PreferredBackBufferHeight = 768
        };
        Content.RootDirectory = "Content";
    }

    protected override void Initialize()
    {
        _rocket.Initialize();

        base.Initialize();
    }

    protected override void LoadContent()
    {
        _spriteBatch = new SpriteBatch(GraphicsDevice);

        _font = Content.Load<SpriteFont>("Fonts/Basic");
        _tiles.Add(Content.Load<Texture2D>("Dialogs/Background"));

        _rocket.LoadContent(Content);
    }

    protected override void Update(GameTime gameTime)
    {
        if (GamePad.GetState(PlayerIndex.One).Buttons.Back == ButtonState.Pressed || Keyboard.GetState().IsKeyDown(Keys.Escape))
            Exit();

        var nextKeyboardState = Keyboard.GetState();
        if (_keyboardState.IsKeyUp(Keys.Enter) &&
            nextKeyboardState.IsKeyDown(Keys.LeftAlt) &&
            nextKeyboardState.IsKeyDown(Keys.Enter))
        {
            _graphics.ToggleFullScreen();
        }
        _keyboardState = nextKeyboardState;

        _rocket.Update(gameTime, _keyboardState);

        base.Update(gameTime);
    }

    protected override void Draw(GameTime gameTime)
    {
        GraphicsDevice.Clear(Color.Black);

        _spriteBatch.Begin();

        _rocket.Draw(_spriteBatch);

        _spriteBatch.DrawString(_font, $"Running: {Math.Round(1000.0f / gameTime.ElapsedGameTime.TotalMilliseconds)}", Vector2.One, Color.White);

        //_spriteBatch.Draw(_tiles[0], new Vector2(100, 100), Color.White);

        _spriteBatch.End();

        base.Draw(gameTime);
    }
}
