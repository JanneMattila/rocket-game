﻿using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Metadata;
using System.Text;
using System.Threading.Tasks;

namespace Rocket;

public class Rocket
{
    private Vector2 _position;
    private float _rotation;
    private Vector2 _velocity;
    private float _speed = 400f;

    private Texture2D _texture;
    internal void Initialize()
    {
        // Set position to center of the screen and velocity to zero
        _position = new Vector2(1024 / 2, 768 / 2);
        _rotation = -MathHelper.PiOver2; // Set rotation to be directly up
        _velocity = Vector2.Zero;
    }

    public void LoadContent(ContentManager content)
    {
        _texture = content.Load<Texture2D>("Ships/Ship01");
    }

    public void Update(GameTime gameTime, KeyboardState keyboardState)
    {
        // Turn rocket by A and Left arrow keys
        if (keyboardState.IsKeyDown(Keys.A) || keyboardState.IsKeyDown(Keys.Left))
        {
            _rotation -= 2f * (float)gameTime.ElapsedGameTime.TotalSeconds;
        }

        // Turn rocket by D and Right arrow keys
        if (keyboardState.IsKeyDown(Keys.D) || keyboardState.IsKeyDown(Keys.Right))
        {
            _rotation += 2f * (float)gameTime.ElapsedGameTime.TotalSeconds;
        }

        // Accelerate by W and Up arrow keys
        if (keyboardState.IsKeyDown(Keys.W) || keyboardState.IsKeyDown(Keys.Up))
        {
            _velocity += new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _speed * (float)gameTime.ElapsedGameTime.TotalSeconds;
        }

        // Decelerate by S and Down arrow keys
        if (keyboardState.IsKeyDown(Keys.S) || keyboardState.IsKeyDown(Keys.Down))
        {
            _velocity -= new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _speed * 0.5f * (float)gameTime.ElapsedGameTime.TotalSeconds;
            if (_velocity.Length() < 0)
            {
                _velocity = Vector2.Zero;
            }
        }

        // Apply air resistance
        _velocity *= 0.99f;

        // Apply gravity
        _velocity.Y += 50f * (float)gameTime.ElapsedGameTime.TotalSeconds;

        // Update position
        _position += _velocity * (float)gameTime.ElapsedGameTime.TotalSeconds;

        // Keep rocket on screen
        if (_position.X < 0)
        {
            _position.X = 0;
            _velocity.X = 0;
        }

        if (_position.X > 1024 - _texture.Width)
        {
            _position.X = 1024 - _texture.Width;
            _velocity.X = 0;
        }

        if (_position.Y < 0)
        {
            _position.Y = 0;
            _velocity.Y = 0;
        }

        if (_position.Y > 768 - _texture.Height)
        {
            _position.Y = 768 - _texture.Height;
            _velocity.Y = 0;
        }
    }


    internal void Draw(SpriteBatch spriteBatch)
    {
        // Draw the rocket with the correct rotation
        spriteBatch.Draw(_texture, _position, null, Color.White, _rotation, new Vector2(_texture.Width / 2, _texture.Height / 2), 1f, SpriteEffects.None, 0f);
    }
}