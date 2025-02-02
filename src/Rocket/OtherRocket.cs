using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using MonoGame.Extended.Input;
using Rocket.Networking;
using RocketShared;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net.Sockets;
using System.Reflection.Metadata;
using System.Text;
using System.Threading.Tasks;

namespace Rocket;

public class OtherRocket
{
    private Vector2 _position;
    private float _rotation;
    private Vector2 _velocity;
    private float _speed = 400f;

    private Vector2 _positionPrevious;
    private float _rotationPrevious;
    private Vector2 _velocityPrevious;
    private float _speedPrevious = 400f;

    private Vector2 _positionShadow;
    private float _rotationShadow;

    private bool _isUp = false;
    private bool _isDown = false;
    private bool _isLeft = false;
    private bool _isRight = false;
    private bool _isFiring = false;

    private readonly List<Shot> _shots = [];
    private int _shotCooldown = 0;

    private int _isActive = 0;
    private NetworkPacket _next = new();
    private List<Vector2> _networkUpdatePositions = new();
    private float _nextLerp = 0.5f;

    private Texture2D _rocketTexture;
    private Texture2D _rocketTexture2;
    private Texture2D _markTexture;

    internal void Initialize()
    {
        // Set position to center of the screen and velocity to zero
        _position = new Vector2(GameSettings.ScreenWidth / 2, GameSettings.ScreenHeight / 2);
        _rotation = -MathHelper.PiOver2; // Set rotation to be directly up
        _velocity = Vector2.Zero;

        _positionShadow = _position;
        _rotationShadow = _rotation;
    }

    public void LoadContent(ContentManager content)
    {
        _rocketTexture = content.Load<Texture2D>("Ships/playerShip3_orange");
        _rocketTexture2 = content.Load<Texture2D>("Ships/playerShip3_blue");
        _markTexture = content.Load<Texture2D>("Ships/Mark");
        Shot.LoadContent(content);
    }

    public void Update(GameTime gameTime)
    {
        float deltaTime = (float)gameTime.ElapsedGameTime.TotalSeconds;
        var updateFromNetwork = false;
        var isFiring = false;
        while (GameNetwork.Incoming.TryDequeue(out var packet))
        {
            _next = packet;

            isFiring = !isFiring && packet.IsFiring == 1 ? true : false;

            //_position = new Vector2(packet.PositionX, packet.PositionY);
            //_velocity = new Vector2(packet.VelocityX, packet.VelocityY);
            //_rotation = packet.Rotation;

            //_positionShadow = new Vector2(packet.PositionX, packet.PositionY);
            //_rotationShadow = packet.Rotation;

            _isActive = 50;
            updateFromNetwork = true;

            _networkUpdatePositions.Add(_position);

            _positionPrevious = _position;
            _velocityPrevious = _velocity;
            _rotationPrevious = _rotation;
            _speedPrevious = _speed;

            _nextLerp = (_nextLerp + deltaTime) - GameSettings.NetworkUpdateTime;
            if (_nextLerp < 0) _nextLerp = 0;
        }

        _isUp = _next.IsUp == 1;
        _isDown = _next.IsDown == 1;
        _isLeft = _next.IsLeft == 1;
        _isRight = _next.IsRight == 1;
        _isFiring = isFiring;

        if (_nextLerp <= GameSettings.NetworkUpdateTime)
        {
            var nextLerp = _nextLerp / GameSettings.NetworkUpdateTime;
            _position.X = MathHelper.Lerp(_positionPrevious.X, _next.PositionX, nextLerp);
            _position.Y = MathHelper.Lerp(_positionPrevious.Y, _next.PositionY, nextLerp);
            _velocity.X = MathHelper.Lerp(_velocityPrevious.X, _next.VelocityX, nextLerp);
            _velocity.Y = MathHelper.Lerp(_velocityPrevious.Y, _next.VelocityY, nextLerp);
            _rotation = MathHelper.Lerp(_rotationPrevious, _next.Rotation, nextLerp);
            _speed = MathHelper.Lerp(_speedPrevious, _next.Speed, nextLerp);
            //_velocity.X = _next.VelocityX;
            //_velocity.Y = _next.VelocityY;
            //_speed = _next.Speed;
            //_rotation = _next.Rotation;

            if (_isFiring)
            {
                // Calculate new shot position
                var shotPosition = _position + new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _rocketTexture.Width / 2;
                _shots.Add(new Shot(shotPosition, _rotation));
            }
        }
        else
        {
            // Turn rocket by A and Left arrow keys
            if (_isLeft)
            {
                _rotation -= 2f * deltaTime;
            }

            // Turn rocket by D and Right arrow keys
            if (_isRight)
            {
                _rotation += 2f * deltaTime;
            }

            // Accelerate by W and Up arrow keys
            if (_isUp)
            {
                _velocity += new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _speed * deltaTime;
                _isUp = true;
            }

            if (_isDown)
            {
            }

            // Add shot by Space key
            if (_shotCooldown > 0) _shotCooldown--;
            if (_isFiring && _shotCooldown == 0)
            {
                // Calculate new shot position
                var shotPosition = _position + new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _rocketTexture.Width / 2;
                _shots.Add(new Shot(shotPosition, _rotation));
                _shotCooldown = 10;
            }

            // Apply air resistance
            _velocity *= 1f - (0.5f * deltaTime);

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

        _isActive--;
        if (_isActive < 0) _isActive = 0;

        _nextLerp += deltaTime;
    }

    internal void Draw(SpriteBatch spriteBatch)
    {
        if (_isActive == 0) return;

        // Draw shots
        foreach (var shot in _shots)
        {
            if (shot.IsActive)
            {
                shot.Draw(spriteBatch);
            }
        }

        // Draw the rocket with the correct rotation
        //spriteBatch.Draw(_rocketTexture2, _positionShadow, null, Color.White, _rotationShadow, new Vector2(_rocketTexture.Width / 2, _rocketTexture.Height / 2), 1f, SpriteEffects.None, 0f);
        spriteBatch.Draw(_rocketTexture, _position, null, Color.White, _rotation, new Vector2(_rocketTexture.Width / 2, _rocketTexture.Height / 2), 1f, SpriteEffects.None, 0f);

        // Draw network update locations
        foreach (var position in _networkUpdatePositions)
        {
            spriteBatch.Draw(_markTexture, position, null, Color.Red, 0, new Vector2(_markTexture.Width / 2, _markTexture.Height / 2), 1f, SpriteEffects.None, 0f);
        }
    }
}
