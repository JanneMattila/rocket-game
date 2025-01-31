using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using MonoGame.Extended.BitmapFonts;
using MonoGame.Extended.Input;
using Rocket.Networking;
using System;
using System.Collections.Generic;

namespace Rocket;

public class Game1 : Game
{
    private GraphicsDeviceManager _graphics;
    private Vector2 _baseScreenSize = new(GameSettings.ScreenWidth, GameSettings.ScreenHeight);
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
    private BitmapFont _bmfont;
    private List<Texture2D> _tiles = [];
    private Texture2D _background;
    private Effect _infinite;
    private Vector2 _xy = new(0f, 0f);
    private float _scale = 1f;
    private float _rotation = 0f;
    private KeyboardState _keyboardState = new();
    private Rocket _rocket = new();
    private OtherRocket _otherRocket = new();

    private bool _isActive = false;

    public Game1()
    {
        _graphics = new GraphicsDeviceManager(this)
        {
            PreferredBackBufferWidth = (int)_baseScreenSize.X,
            PreferredBackBufferHeight = (int)_baseScreenSize.Y,
            IsFullScreen = false,
            GraphicsProfile = GraphicsProfile.HiDef
        };

        _graphics.SynchronizeWithVerticalRetrace = false; // Disable VSync
        IsFixedTimeStep = false; // Unlock framerate

        //_graphics.PreparingDeviceSettings += (sender, e) =>
        //{
        //    e.GraphicsDeviceInformation.PresentationParameters.PresentationInterval = PresentInterval.Two;
        //};
        Content.RootDirectory = "Content";
        //TargetElapsedTime = TimeSpan.FromSeconds(1d / 30d);
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

    private Matrix GetView()
    {
        int width = GraphicsDevice.Viewport.Width;
        int height = GraphicsDevice.Viewport.Height;
        Vector2 origin = new(width / 2f, height / 2f);

        return
            Matrix.CreateTranslation(-origin.X, -origin.Y, 0f) *
            Matrix.CreateTranslation(-_xy.X, -_xy.Y, 0f) *
            Matrix.CreateRotationZ(_rotation) *
        Matrix.CreateScale(_scale, _scale, 1f) *
            Matrix.CreateTranslation(origin.X, origin.Y, 0f);
    }
    private Matrix GetUVTransform(Texture2D t, Vector2 offset, float scale, Viewport v)
    {
        return
            Matrix.CreateScale(t.Width, t.Height, 1f) *
            Matrix.CreateScale(scale, scale, 1f) *
            Matrix.CreateTranslation(offset.X, offset.Y, 0f) *
            GetView() *
            Matrix.CreateScale(1f / v.Width, 1f / v.Height, 1f);
    }
    #endregion

    protected override void Initialize()
    {
        _rocket.Initialize();
        _otherRocket.Initialize();

        base.Initialize();
    }

    protected override void UnloadContent()
    {
        GameNetwork.Status = GameNetworkStatus.Disconnected;

        base.UnloadContent();
    }

    protected override void LoadContent()
    {
        _spriteBatch = new SpriteBatch(GraphicsDevice);

        _bmfont = BitmapFont.FromFile(_graphics.GraphicsDevice, "Content/Fonts/BitmapFont01.fnt");

        _basicFont = Content.Load<SpriteFont>("Fonts/BasicFont");
        _dialogFont = Content.Load<SpriteFont>("Fonts/DialogFont");

        _tiles.Add(Content.Load<Texture2D>("Dialogs/Background"));

        _background = Content.Load<Texture2D>("Backgrounds/Space01");
        //_infinite = Content.Load<Effect>("Backgrounds/Infinite");

        _rocket.LoadContent(Content);
        _otherRocket.LoadContent(Content);

        ScalePresentationArea();
    }

    protected override void Update(GameTime gameTime)
    {
        if (GamePad.GetState(PlayerIndex.One).Buttons.Back == ButtonState.Pressed || Keyboard.GetState().IsKeyDown(Keys.Escape))
            Exit();

        KeyboardExtended.Update();
        var keyboardState = KeyboardExtended.GetState();
        if (keyboardState.WasKeyPressed(Keys.F11) ||
            keyboardState.WasKeyPressed(Keys.Enter) && keyboardState.IsKeyDown(Keys.LeftAlt))
        {
            ToggleFullscreen();
        }

        if (_backBufferHeight != GraphicsDevice.PresentationParameters.BackBufferHeight ||
            _backBufferWidth != GraphicsDevice.PresentationParameters.BackBufferWidth)
        {
            ScalePresentationArea();
        }

        if (_isActive && GameNetwork.Status == GameNetworkStatus.Connected)
        {
            _rocket.Update(gameTime, keyboardState);
            _otherRocket.Update(gameTime);
        }
        else if (GameNetwork.Status == GameNetworkStatus.Connected)
        {
            _isActive = true;
        }

        base.Update(gameTime);
    }

    protected override void Draw(GameTime gameTime)
    {
        GraphicsDevice.Clear(Color.Black);

        //int width = GraphicsDevice.Viewport.Width;
        //int height = GraphicsDevice.Viewport.Height;

        //Matrix projection = Matrix.CreateOrthographicOffCenter(0, width, height, 0, 0, 1);
        //Matrix uv_transform = GetUVTransform(_background, new Vector2(0, 0), 1f, GraphicsDevice.Viewport);

        //_infinite.Parameters["view_projection"].SetValue(Matrix.Identity * projection);
        //_infinite.Parameters["uv_transform"].SetValue(Matrix.Invert(uv_transform));

        //_spriteBatch.Begin(effect: _infinite, samplerState: SamplerState.LinearWrap);
        //_spriteBatch.Draw(_background, GraphicsDevice.Viewport.Bounds, Color.White);
        //_spriteBatch.End();

        _spriteBatch.Begin(SpriteSortMode.Deferred, null, null, null, null, null, _globalTransformation);

        _rocket.Draw(_spriteBatch);
        _otherRocket.Draw(_spriteBatch);

        _spriteBatch.DrawString(_basicFont, $"Running: {Math.Round(1000.0f / gameTime.ElapsedGameTime.TotalMilliseconds)}", Vector2.One, Color.White);

        if (!_isActive)
        {
            _spriteBatch.Draw(_tiles[0], new Vector2(50, 50), Color.White);
            _spriteBatch.DrawString(_bmfont, GameNetworkStatus.Connected.ToString(), new Vector2(350, 350), Color.White);
        }

        //_spriteBatch.DrawString(_bmfont, $"Connecting to server...", new Vector2(350, 15), Color.White);
        _spriteBatch.End();

        base.Draw(gameTime);
    }
}
