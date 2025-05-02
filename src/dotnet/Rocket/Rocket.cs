using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using MonoGame.Extended.BitmapFonts;
using MonoGame.Extended.Input;
using Rocket.Networking;
using RocketShared;
using System;
using System.Collections.Generic;
namespace Rocket;

public class Rocket
{
    private Vector2 _position;
    private float _rotation;
    private Vector2 _velocity;
    private float _speed = 400f;

    private bool _isUp = false;
    private bool _isDown = false;
    private bool _isLeft = false;
    private bool _isRight = false;
    private bool _isFiring = false;
    private bool _isFiringNotSent = false;

    private readonly List<Shot> _shots = [];
    private float _shotCooldown = 0;

    private float _networkUpdateTime = 0;

    private Texture2D _rocketTexture;
    private SpriteFont _basicFont;

    internal void Initialize()
    {
        // Set position to center of the screen and velocity to zero
        _position = new Vector2(GameSettings.ScreenWidth / 2, GameSettings.ScreenHeight / 2);
        _rotation = -MathHelper.PiOver2; // Set rotation to be directly up
        _velocity = Vector2.Zero;
    }

    public void LoadContent(ContentManager content)
    {
        _rocketTexture = content.Load<Texture2D>("Ships/playerShip3_red");
        _basicFont = content.Load<SpriteFont>("Fonts/BasicFont");

        Shot.LoadContent(content);
    }

    public void Update(GameTime gameTime, KeyboardStateExtended keyboardState)
    {
        float deltaTime = (float)gameTime.ElapsedGameTime.TotalSeconds;

        // Turn rocket by A and Left arrow keys
        var isUp = false;
        var isDown = false;
        var isLeft = false;
        var isRight = false;
        var isFiring = false;

        if (keyboardState.IsKeyDown(Keys.A) || keyboardState.IsKeyDown(Keys.Left))
        {
            _rotation -= 2f * deltaTime;
            isLeft = true;
        }

        // Turn rocket by D and Right arrow keys
        if (keyboardState.IsKeyDown(Keys.D) || keyboardState.IsKeyDown(Keys.Right))
        {
            _rotation += 2f * deltaTime;
            isRight = true;
        }

        // Accelerate by W and Up arrow keys
        if (keyboardState.IsKeyDown(Keys.W) || keyboardState.IsKeyDown(Keys.Up))
        {
            _velocity += new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _speed * deltaTime;
            isUp = true;
        }

        if (keyboardState.IsKeyDown(Keys.S) || keyboardState.IsKeyDown(Keys.Down))
        {
            isDown = true;
        }

        // Add shot by Space key
        if (_shotCooldown > 0) _shotCooldown -= 50f * deltaTime;
        if (keyboardState.IsKeyDown(Keys.Space) && _shotCooldown <= 0)
        {
            // Calculate new shot position
            var shotPosition = _position + new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _rocketTexture.Width / 2;
            _shots.Add(new Shot(shotPosition, _rotation));
            _shotCooldown += 10f;
            isFiring = true;
        }

        var shotsToRemove = new List<Shot>();
        foreach (var shot in _shots)
        {
            shot.Update(gameTime);
            if (!shot.IsActive)
            {
                shotsToRemove.Add(shot);
            }
        }

        foreach (var shot in shotsToRemove)
        {
            _shots.Remove(shot);
        }

        // Apply air resistance
        _velocity *= 1f - (0.5f *  deltaTime);

        // Apply gravity
        _velocity.Y += 50f * deltaTime;

        // Update position
        _position += _velocity * deltaTime;

        // Keep rocket on screen
        if (_position.X < _rocketTexture.Width / 2)
        {
            _position.X = _rocketTexture.Width / 2;
            _velocity.X = 0;
        }

        if (_position.X > GameSettings.ScreenWidth - _rocketTexture.Width / 2)
        {
            _position.X = GameSettings.ScreenWidth - _rocketTexture.Width / 2;
            _velocity.X = 0;
        }

        if (_position.Y < _rocketTexture.Height / 2)
        {
            _position.Y = _rocketTexture.Height / 2;
            _velocity.Y = 0;
        }

        if (_position.Y > GameSettings.ScreenHeight - _rocketTexture.Height / 2)
        {
            _position.Y = GameSettings.ScreenHeight - _rocketTexture.Height / 2;
            _velocity.Y = 0;
        }

        _isFiringNotSent = _isFiringNotSent || isFiring;
        if (_networkUpdateTime >= GameSettings.NetworkUpdateTime)
        {
            GameNetwork.Outgoing.Enqueue(new NetworkPacket()
            {
                PositionX = _position.X,
                PositionY = _position.Y,
                VelocityX = _velocity.X,
                VelocityY = _velocity.Y,
                Rotation = _rotation,
                Speed = _speed,
                Delta = deltaTime,
                IsUp = isUp ? (byte)1 : (byte)0,
                IsDown = isDown ? (byte)1 : (byte)0,
                IsLeft = isLeft ? (byte)1 : (byte)0,
                IsRight = isRight ? (byte)1 : (byte)0,
                IsFiring = _isFiringNotSent ? (byte)1 : (byte)0
            });

            _isFiringNotSent = false;
            if (GameNetwork.Outgoing.Count > 1)
            {
                GameNetwork.Outgoing.TryDequeue(out var previous);
                if (previous.IsFiring == 1)
                {
                    _isFiringNotSent = true;
                }
            }
            _networkUpdateTime -= GameSettings.NetworkUpdateTime;
        }

        _networkUpdateTime += deltaTime;
    }


    internal void Draw(SpriteBatch spriteBatch)
    {
        // Draw shots
        foreach (var shot in _shots)
        {
            if (shot.IsActive)
            {
                shot.Draw(spriteBatch);
            }
        }

        // Draw the rocket with the correct rotation
        spriteBatch.Draw(_rocketTexture, _position, null, Color.White, _rotation, new Vector2(_rocketTexture.Width / 2, _rocketTexture.Height / 2), 1f, SpriteEffects.None, 0f);

        //spriteBatch.DrawString(_basicFont, $"Velocity: {_velocity}", new Vector2(350, 15), Color.White);
    }
}
