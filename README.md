# CC3200-Communicating-Over-Wifi

A fully functional T-9 Texting scheme that augmented the existing two-way board-to-board communication routine we created. Instead of having to connect the two boards via UART we can now send and receive messages between the two boards wirelessly via a secure MQTT cloud service hosted by Amazon Web Services. Now in order to send a message to the other board all you need to do is plug in your CC3200, allow it to connect to the wireless access point it’s configured to, and then send your message. This message will be published to the MQTT topic that is specified in the code and any devices that are subscribed to that topic will receive that message and display it on their OLED.

Uses an Adrafruit OLED Breakout Board - 16-bit Color 1.5" (product id: 1431), a Texas Instruments CC3200 LaunchPad (CC3200-LAUNCHXL), a Vishay TSOP31336 IR receiver module, and an AT&T S10-S3 Remote Control.

The general design process involved the following stages: 
1.	Setting up an Amazon Web Services account.
2.	Creating a Device shadow for both CC3200 boards.
3.	Generate certificates and keys for encrypted communication and flash those certificates and keys onto both of our C3200 boards after converting them to the required format (.der format).
4.	Update the server address, port number, time, etc. and make sure that the code we were provided will ensure that our connection will be secure.
5.	Integrate the IR signal decoding code from Lab Assignment 3 into the MQTT secure client code.
6.	Modify the “Mqtt_Recv()” function so that it will now display the payload onto the lower half (receiver side) of the OLED display.
7.	Modify the “MqttClient()” function so that it will now publish the message the user has typed onto the OLED using the IR Remote when he or she hits the ‘ENTER’ key (which was the ‘CH+’ key for us).


Here’s an outline of our code and the process by which it sends and receives messages over the AWS MQTT broker. 

Connecting to MQTT
1.	Inside the main() function we start the MQTT client task.
2.	In the MqttClient() function we reset the state of the machine, start the driver, and connet to the access point using the security parameters that we provided in the “common.h” file.
3.	If we successfully connected, we then initialize the MQTT client lib, connect to our MQTT broker (AWS), and subscribe the specified topics (which are hard-coded at the top of the code). 
4.	We then set up all the interrupts and handlers that are required to detect and decode the IR signals.
5.	We then enter an infinite for loop and begin reading in characters from the IR Remote.

Publishing a Message
1.	Inside the MqttClient() function we wait inside an infinite for loop for the IR receiver to detect and decode an incoming pulse from the IR remote. 
2.	If that button pressed is valid, we process it and print it to the sender side of the OLED display (the top half). Note that the user can erase any errant characters using the ‘CH-‘ key (which functions as out ‘DELETE’ key).
3.	Once the user has finished typing their message they press the ‘CH+’ key (which functions as our ‘ENTER’ key). 
4.	This signals to the code that we should transmit the stored message (which we’ve been buffering inside a char* called sendBuffer) and we then use the sl_ExtLib_MqttClientSend() function to publish our message to the publish topic we hard-coded at the top (PUB_TOPIC) .

Receiving a Message
1.	When we detect that a message has been published to the topic(s) we’re subscribed to (SUB_TOPIC  which we hard-coded at the top of the code) we use the event handler, Mqtt_Recv(), to process the incoming message.
2.	Inside Mqtt_Recv(), we take the payload, convert it into an output string and display it to the receiver side of the OLED display (the lower half).
3.	When another message is received we clear the lower half of the OLED and then print the new message.

While texting over MQTT is interesting, we decided to put a slight twist on it by also incorporating an Easter egg/Mini-game in the form of a Tic-Tac-Toe game that can be initialized and played between two players wirelessly over MQTT. 

	How to Start the Game
1.	When someone wants to start the Tic-Tac-Toe Mini-game all they need to do is press the ‘MUTE’ key on the IR Remote. This key has been assigned as the “TIC_TAC_TOE_CODE” which signals that we should initiate Tic-Tac-Toe.
2.	Once pressed we’ll send a special message over MQTT that signals to the receiver that they should also initialize Tic-Tac-Toe. Note that the person that initially pressed the ‘MUTE’ key and started the game will be player X and the person who receives the message will be player O.
3.	At this point both player’s boards will configure their respective OLEDs and display the Tic-Tac-Toe game board and instructions for playing the game.

How to Play the Game 
1.	The game is played like any other Tic-Tac-Toe game with each player taking turns. Note that you select your choice using the number keys (‘1’ to ‘9’).
2.	Your game board will display whether or not it’s your turn. If it’s your turn you’re free to select your choice. If not, you’ll need to wait until you receive your opponent’s choice.
3.	When someone wins or if there is a tie the board will reset back into standard IR Texting mode and the result will be displayed on the lower portion (receiver side) of the board.

How it Works/Code Outline
1.	Our code now has two distinct modes: the standard IR Texting mode and the Tic-Tac-Toe mode. To toggle from IR texting mode to Tic-Tac-Toe mode you simply need to press the ‘MUTE’ key (which we have hard coded to represent the Tic-Tac-Toe game). Once the game finishes you’ll automatically revert back to IR texting mode.
2.	If you’re in Tic-Tac-Toe mode, it’ll either be your turn or your opponents (we use a flag called “your_turn”). If it’s your turn you’re able to select your choice. Once selected you’ll transmit that choice over MQTT, check if that choice results in your victory, and update the board accordingly. If it’s the opponents turn you’re unable to select any choices and will instead be forced wait for your opponent to transmit their message. Once they do you’ll update your game board and check if they won. If they didn’t then we now set the “your_turn” flag and you’re free to select your choice. This continues until someone wins or until we get a draw. 
3.	The game state, whose turn it is, whether or not we’re in Tic-Tac-Toe mode, and any other features related to the Tic-Tac-Toe Mini-game are stored in a series of flags. These flags are used to control the logic and determine if we’re in Tic-Tac-Toe mode, if the user can transmit their choice, and the general state of the game board.

