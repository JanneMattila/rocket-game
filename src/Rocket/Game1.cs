using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using System;
using System.Collections.Generic;

namespace Rocket;

public class Game1 : Game
{
    private GraphicsDeviceManager _graphics;
    private Vector2 _baseScreenSize = new(1024, 768);
    private Matrix _globalTransformation;
    private bool _isFullscreen = false;
    private bool _isBorderless = false;
    private int _width = 0;
    private int _height = 0;
    private int _backBufferWidth;
    private int _backBufferHeight;

    private SpriteBatch _spriteBatch;
    private SpriteFont _basicFont;
    private SpriteFont _dialogFont;
    private List<Texture2D> _tiles = [];
    private KeyboardState _keyboardState = new();
    private Rocket _rocket = new();

    private bool _isActive = true;

    public Game1()
    {
        _graphics = new GraphicsDeviceManager(this)
        {
            PreferredBackBufferWidth = (int)_baseScreenSize.X,
            PreferredBackBufferHeight = (int)_baseScreenSize.Y,
            IsFullScreen = false
        };
        Content.RootDirectory = "Content";
    }

    #region Fullscreen and borderless window support

    // Fullscreen and borderless window support from:
    // https://learn-monogame.github.io/how-to/fullscreen/
    public void ToggleFullscreen()
    {
        bool oldIsFullscreen = _isFullscreen;

        if (_isBorderless)
        {
            _isBorderless = false;
        }
        else
        {
            _isFullscreen = !_isFullscreen;
        }

        ApplyFullscreenChange(oldIsFullscreen);
    }

    public void ToggleBorderless()
    {
        bool oldIsFullscreen = _isFullscreen;

        _isBorderless = !_isBorderless;
        _isFullscreen = _isBorderless;

        ApplyFullscreenChange(oldIsFullscreen);
    }

    private void ApplyFullscreenChange(bool oldIsFullscreen)
    {
        if (_isFullscreen)
        {
            if (oldIsFullscreen)
            {
                ApplyHardwareMode();
            }
            else
            {
                SetFullscreen();
            }
        }
        else
        {
            UnsetFullscreen();
        }
    }

    private void ApplyHardwareMode()
    {
        _graphics.HardwareModeSwitch = !_isBorderless;
        _graphics.ApplyChanges();
    }

    private void SetFullscreen()
    {
        _width = Window.ClientBounds.Width;
        _height = Window.ClientBounds.Height;

        _graphics.PreferredBackBufferWidth = GraphicsAdapter.DefaultAdapter.CurrentDisplayMode.Width;
        _graphics.PreferredBackBufferHeight = GraphicsAdapter.DefaultAdapter.CurrentDisplayMode.Height;
        _graphics.HardwareModeSwitch = !_isBorderless;

        _graphics.IsFullScreen = true;
        _graphics.ApplyChanges();
    }

    private void UnsetFullscreen()
    {
        _graphics.PreferredBackBufferWidth = _width;
        _graphics.PreferredBackBufferHeight = _height;
        _graphics.IsFullScreen = false;
        _graphics.ApplyChanges();
    }

    // From PlatformerGame example
    public void ScalePresentationArea()
    {
        //Work out how much we need to scale our graphics to fill the screen
        _backBufferWidth = GraphicsDevice.PresentationParameters.BackBufferWidth;
        _backBufferHeight = GraphicsDevice.PresentationParameters.BackBufferHeight;
        float horScaling = _backBufferWidth / _baseScreenSize.X;
        float verScaling = _backBufferHeight / _baseScreenSize.Y;
        Vector3 screenScalingFactor = new Vector3(horScaling, verScaling, 1);
        _globalTransformation = Matrix.CreateScale(screenScalingFactor);
    }
    #endregion

    protected override void Initialize()
    {
        _rocket.Initialize();

        base.Initialize();
    }

    protected override void LoadContent()
    {
        _spriteBatch = new SpriteBatch(GraphicsDevice);

        _basicFont = Content.Load<SpriteFont>("Fonts/BasicFont");
        _dialogFont = Content.Load<SpriteFont>("Fonts/DialogFont");
        _tiles.Add(Content.Load<Texture2D>("Dialogs/Background"));

        _rocket.LoadContent(Content);

        ScalePresentationArea();
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
            ToggleFullscreen();
        }
        _keyboardState = nextKeyboardState;

        if (_backBufferHeight != GraphicsDevice.PresentationParameters.BackBufferHeight ||
            _backBufferWidth != GraphicsDevice.PresentationParameters.BackBufferWidth)
        {
            ScalePresentationArea();
        }

        if (_isActive)
        {
            _rocket.Update(gameTime, _keyboardState);
        }

        base.Update(gameTime);
    }

    protected override void Draw(GameTime gameTime)
    {
        GraphicsDevice.Clear(Color.Black);

        _spriteBatch.Begin(SpriteSortMode.Deferred, null, null, null, null, null, _globalTransformation);

        _rocket.Draw(_spriteBatch);

        _spriteBatch.DrawString(_basicFont, $"Running: {Math.Round(1000.0f / gameTime.ElapsedGameTime.TotalMilliseconds)}", Vector2.One, Color.White);

        if (!_isActive)
        {
            _spriteBatch.Draw(_tiles[0], new Vector2(50, 50), Color.White);
            _spriteBatch.DrawString(_dialogFont, $"Connecting to server...", new Vector2(300, 350), Color.White);
        }

        _spriteBatch.End();

        base.Draw(gameTime);
    }
}
