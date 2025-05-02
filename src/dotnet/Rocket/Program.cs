using Microsoft.Extensions.Configuration;
using Rocket;
using Rocket.Networking;
using System;
using System.Threading.Tasks;

var builder = new ConfigurationBuilder()
    .AddJsonFile("appsettings.json", optional: true);

var configuration = builder.Build();

var server = configuration.GetValue("server", "127.0.0.1");
var port = configuration.GetValue("port", 3501);

ArgumentNullException.ThrowIfNull(server);

var gameNetwork = new GameNetwork(port, server);

// Run gameNetwork in a separate thread
#pragma warning disable CS4014 // Because this call is not awaited, execution of the current method continues before the call is completed
Task.Run(gameNetwork.RunNetworking);
#pragma warning restore CS4014 // Because this call is not awaited, execution of the current method continues before the call is completed

using var game = new Game1();
game.Run();
