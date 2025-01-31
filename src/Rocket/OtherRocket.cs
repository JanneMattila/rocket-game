﻿using Microsoft.Xna.Framework;
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
    private float _nextLerp = 0.5f;

    private Texture2D _rocketTexture;
    private Texture2D _rocketTexture2;

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
        Shot.LoadContent(content);
    }

    public void Update(GameTime gameTime)
    {
        var updateFromNetwork = true;
        var isFiring = false;
        while (GameNetwork.Incoming.TryDequeue(out var packet))
        {
            _next = packet;
            _nextLerp = 0;

            isFiring = !isFiring && packet.IsFiring == 1 ? true : false;

            _position = new Vector2(packet.PositionX, packet.PositionY);
            _velocity = new Vector2(packet.VelocityX, packet.VelocityY);
            _rotation = packet.Rotation;

            _positionShadow = new Vector2(packet.PositionX, packet.PositionY);
            _rotationShadow = packet.Rotation;

            _isActive = 50;
            updateFromNetwork = true;
        }

        //_position.X = MathHelper.Lerp(_position.X, _next.PositionX, _nextLerp);
        //_position.Y = MathHelper.Lerp(_position.Y, _next.PositionY, _nextLerp);
        //_velocity.X = MathHelper.Lerp(_velocity.X, _next.VelocityX, _nextLerp);
        //_velocity.Y = MathHelper.Lerp(_velocity.Y, _next.VelocityY, _nextLerp);
        //_speed = MathHelper.Lerp(_speed, _next.Speed, _nextLerp);
        //_rotation = MathHelper.Lerp(_rotation, _next.Rotation, _nextLerp);
        //_velocity.X = _next.VelocityX;
        //_velocity.Y = _next.VelocityY;
        //_speed = _next.Speed;
        //_rotation = _next.Rotation;

        _isUp = _next.IsUp == 1;
        _isDown = _next.IsDown == 1;
        _isLeft = _next.IsLeft == 1;
        _isRight = _next.IsRight == 1;
        _isFiring = isFiring;
        _nextLerp += 1 / GameSettings.NetworkUpdateTime;
        Debug.WriteLine(_nextLerp);
        updateFromNetwork = true;

        if (updateFromNetwork)
        {
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
                _rotation -= 2f * (float)gameTime.ElapsedGameTime.TotalSeconds;
            }

            // Turn rocket by D and Right arrow keys
            if (_isRight)
            {
                _rotation += 2f * (float)gameTime.ElapsedGameTime.TotalSeconds;
            }

            // Accelerate by W and Up arrow keys
            if (_isUp)
            {
                _velocity += new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _speed * (float)gameTime.ElapsedGameTime.TotalSeconds;
                _isUp = true;
            }

            // Decelerate by S and Down arrow keys
            if (_isDown)
            {
                _velocity -= new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _speed * 0.5f * (float)gameTime.ElapsedGameTime.TotalSeconds;
                _isDown = true;
                if (_velocity.Length() < 0)
                {
                    _velocity = Vector2.Zero;
                }
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
            _velocity *= 0.99f;

            // Apply gravity
            _velocity.Y += 50f * (float)gameTime.ElapsedGameTime.TotalSeconds;

            // Update position
            _position += _velocity * (float)gameTime.ElapsedGameTime.TotalSeconds;

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
        spriteBatch.Draw(_rocketTexture2, _positionShadow, null, Color.White, _rotationShadow, new Vector2(_rocketTexture.Width / 2, _rocketTexture.Height / 2), 1f, SpriteEffects.None, 0f);
        spriteBatch.Draw(_rocketTexture, _position, null, Color.White, _rotation, new Vector2(_rocketTexture.Width / 2, _rocketTexture.Height / 2), 1f, SpriteEffects.None, 0f);
    }
}
