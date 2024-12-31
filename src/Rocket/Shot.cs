using Microsoft.Xna.Framework.Content;
using Microsoft.Xna.Framework.Graphics;
using Microsoft.Xna.Framework.Input;
using Microsoft.Xna.Framework;
using System.Collections.Generic;
using System;
using MonoGame.Extended.Input;

namespace Rocket;

public class Shot
{
    private Vector2 _position;
    private float _rotation;
    private float _speed = 1200f;

    private bool _isActive = true;
    public bool IsActive => _isActive;

    private static Texture2D _texture;

    internal Shot(Vector2 position, float rotation)
    {
        _position = position;
        _rotation = rotation;
    }

    public static void LoadContent(ContentManager content)
    {
        _texture = content.Load<Texture2D>("Ships/Shot");
    }

    public void Update(GameTime gameTime)
    {
        // Move shot in direction of rotation
        _position += new Vector2((float)Math.Cos(_rotation), (float)Math.Sin(_rotation)) * _speed * (float)gameTime.ElapsedGameTime.TotalSeconds;

        // Keep rocket on screen
        if (_position.X < 0 - _texture.Width ||
            _position.X > 1024 + _texture.Width ||
            _position.Y < 0 - _texture.Height ||
            _position.Y > 768 + _texture.Height)
        {
            _isActive = false;
        }
    }


    internal void Draw(SpriteBatch spriteBatch)
    {
        // Draw the rocket with the correct rotation
        spriteBatch.Draw(_texture, _position, null, Color.White, _rotation, new Vector2(_texture.Width / 2, _texture.Height / 2), 1f, SpriteEffects.None, 0f);
    }
}