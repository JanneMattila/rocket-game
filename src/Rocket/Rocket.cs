using Microsoft.Xna.Framework;
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

    private readonly List<Shot> _shots = [];

    private Texture2D _rocketTexture;

    internal void Initialize()
    {
        // Set position to center of the screen and velocity to zero
        _position = new Vector2(1024 / 2, 768 / 2);
        _rotation = -MathHelper.PiOver2; // Set rotation to be directly up
        _velocity = Vector2.Zero;
    }

    public void LoadContent(ContentManager content)
    {
        _rocketTexture = content.Load<Texture2D>("Ships/Ship01");
        Shot.LoadContent(content);
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

        // Add shot by Space key
        if (keyboardState.IsKeyDown(Keys.Space))
        {
            _shots.Add(new Shot(_position, _rotation));
        }

        var shotsToRemove = new List<Shot>();
        foreach (var shot in _shots)
        {
            shot.Update(gameTime, keyboardState);
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

        if (_position.X > 1024 - _rocketTexture.Width / 2)
        {
            _position.X = 1024 - _rocketTexture.Width / 2;
            _velocity.X = 0;
        }

        if (_position.Y < _rocketTexture.Height / 2)
        {
            _position.Y = _rocketTexture.Height / 2;
            _velocity.Y = 0;
        }

        if (_position.Y > 768 - _rocketTexture.Height / 2)
        {
            _position.Y = 768 - _rocketTexture.Height / 2;
            _velocity.Y = 0;
        }
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
    }
}
