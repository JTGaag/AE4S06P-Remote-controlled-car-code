package com.delftsoft.arduinobluetoothcontroller;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Set;
import java.util.UUID;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnKeyListener;
import android.view.View.OnTouchListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.PopupMenu;
import android.widget.PopupMenu.OnMenuItemClickListener;
import android.widget.TextView;

public class MainActivity extends Activity implements OnMenuItemClickListener{

	//initiate adapter BTadapter and BTdevice
	BluetoothAdapter mBluetoothAdapter;
	BluetoothDevice mmDevice;
	BluetoothSocket mmSocket;
	OutputStream mmOutputStream;
	InputStream mmInputStream;
	Thread workerThread;
	ArrayList<BluetoothDevice> mmDevices = new ArrayList<BluetoothDevice>();
	
	PopupMenu popupMenu;
	
	Button buttonConnect;
	Button buttonOpen;
	Button buttonClose;
	Button buttonChat;
	TextView myLabel;
	TextView btStatus;
	TextView pairedDevice;
	EditText editText;
	
	//Movement images
	ImageView arrow_up;
	ImageView arrow_left;
	ImageView arrow_right;
	ImageView arrow_down;
	
	//Movement input booleans
	static volatile boolean move_up = false;
	static volatile boolean move_left = false;
	static volatile boolean move_right = false;
	static volatile boolean move_down = false;
	
	//Handler and runnable initiation
	Handler handler;
	Runnable runnable;
	
	//BT variables
	byte[] readBuffer;
    int readBufferPosition;
    int counter;
    
    volatile boolean stopWorker;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		Log.d("Move Debug","move_up: "+Boolean.toString(move_up));
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main_control);
		
		buttonConnect = (Button)findViewById(R.id.buttonConnect);
		buttonOpen = (Button)findViewById(R.id.buttonOpen);
		buttonClose = (Button)findViewById(R.id.buttonClose);
		buttonChat = (Button)findViewById(R.id.buttonChat);
		myLabel = (TextView)findViewById(R.id.myLabel);
		btStatus = (TextView)findViewById(R.id.textViewBTStatus);
		pairedDevice = (TextView)findViewById(R.id.textViewPairedDevice);
		editText = (EditText)findViewById(R.id.editText);
		
		arrow_up = (ImageView)findViewById(R.id.arrowUp);
		arrow_left = (ImageView)findViewById(R.id.arrowLeft);
		arrow_right = (ImageView)findViewById(R.id.arrowRight);
		arrow_down = (ImageView)findViewById(R.id.arrowDown);
		
		handler = new Handler();
		
		//Runnable to send information about key pressed via bluetooth to car
		runnable = new Runnable(){

			@Override
			public void run() {
				//Log.d("Move Debug","move_up: "+Boolean.toString(move_up));
				if(move_up == false && move_left == false && move_right == false && move_down == false){
					sendMovement("s");
				}
				if(move_up == true){
					sendMovement("f");
				}
				if(move_left == true){
					sendMovement("l");
				}
				if(move_right == true){
					sendMovement("r");
				}
				if(move_down == true){
					sendMovement("b");
				}
				handler.postDelayed(runnable, 10);
			}
			
		};
		//Start runnable
		handler.postDelayed(runnable, 10);
		////////////////////////////////////////////////////////////////
		//
		//EditText keylistner
		//
		///////////////////////////////////////////////////////////////
		editText.setOnKeyListener(new OnKeyListener() {
			public boolean onKey(View v, int keyCode, KeyEvent event) {
				// If the event is a key-down event on the "enter" button
				if ((event.getAction() == KeyEvent.ACTION_DOWN) && (keyCode == KeyEvent.KEYCODE_ENTER)) {
					sendData(); 
					editText.setText("", TextView.BufferType.EDITABLE);
					return true;
				}
				return false;
			}
		});
		
		////////////////////////////////////////////////////////////////
		//
		//Other lsiteners
		//
		///////////////////////////////////////////////////////////////		
		setImageListeners();
		setButtonListeners();
		
        
        
	}

	
	
	
	@Override
	protected void onResume() {
		super.onResume();		
	}


	@Override
	protected void onPause() {
		super.onPause();
	}

	public void connectBTDevice(){
		//Connect to bluetooth device
		// TODO: Connect with MAC-Address directly
		Set<BluetoothDevice> pairedDevices = mBluetoothAdapter.getBondedDevices();
		if(pairedDevices.size() > 0)
		{
			int deviceId = 0;
			popupMenu = new PopupMenu(this, findViewById(R.id.myLabel));
			for(BluetoothDevice device : pairedDevices)
			{
				mmDevices.add(deviceId,device);
				popupMenu.getMenu().add(Menu.NONE,deviceId,Menu.NONE,device.getName());
				deviceId++;

			}
			popupMenu.setOnMenuItemClickListener(this);
			popupMenu.show();
		}
	}

	public void openBTConnection(){
		if(mmDevice!=null){
			try {
				//Make socket, in and output stream
				UUID uuid = UUID.fromString("00001101-0000-1000-8000-00805f9b34fb"); //Standard SerialPortService ID
				mmSocket = mmDevice.createRfcommSocketToServiceRecord(uuid);
				mmSocket.connect();
				mmOutputStream = mmSocket.getOutputStream();
				mmInputStream = mmSocket.getInputStream();
				myLabel.setText("Connection opened");
				btStatus.setText("Connected (open)");
				} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			beginListenForData();
		}
	}
	
	public void closeBTConnection(){
		stopWorker = true;
		if(mmOutputStream != null && mmInputStream != null && mmSocket != null){
			try {
				mmOutputStream.close();
				mmInputStream.close();
				mmSocket.close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
	        myLabel.setText("Bluetooth Closed");
	        btStatus.setText("Connection Closed");
		}
	}
	
	public void sendData(){
		if(mmOutputStream != null){
	        String msg = editText.getText().toString();
	        msg += "\n";
	        try {
				mmOutputStream.write(msg.getBytes());
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}else{
			myLabel.setText("No open connection");
		}
	}
	
	public void sendMovement(String s){
		if(mmOutputStream != null){
			myLabel.setText(s);
	        s += "\n";
	        try {
				mmOutputStream.write(s.getBytes());
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}else{
			//myLabel.setText("No open connection");
		}
	}
	
	public void beginListenForData(){
		final Handler handler = new Handler();
		final byte delimiter = 10; //This is the ASCII code for a newline character
        
        stopWorker = false;
        readBufferPosition = 0;
        readBuffer = new byte[1024];
		
        workerThread = new Thread(new Runnable()
        {
            public void run()
            {
               while(!Thread.currentThread().isInterrupted() && !stopWorker)
               {
            	   try {
					int bytesAvailable = mmInputStream.available();
					   if(bytesAvailable > 0)
					   {
					       byte[] packetBytes = new byte[bytesAvailable];
					       mmInputStream.read(packetBytes);
					       
					       for(int i=0;i<bytesAvailable;i++)
                           {
                               byte b = packetBytes[i];
                               if(b == delimiter)
                               {
                                   byte[] encodedBytes = new byte[readBufferPosition];
                                   System.arraycopy(readBuffer, 0, encodedBytes, 0, encodedBytes.length);
                                   final String data = new String(encodedBytes, "US-ASCII");
                                   readBufferPosition = 0;

                                   //The variable data now contains our full command
                                   
                                   handler.post(new Runnable()
                                   {
                                       public void run()
                                       {
                                           myLabel.setText(data);
                                           Log.d("Data", data);
                                       }
                                   });
                               }
                               else
                               {
                                   readBuffer[readBufferPosition++] = b;
                               }
                           }
					   }
				} catch (IOException e) {
					stopWorker = true;
					e.printStackTrace();
				}
               }
            }
        });
        workerThread.start();
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle action bar item clicks here. The action bar will
		// automatically handle clicks on the Home/Up button, so long
		// as you specify a parent activity in AndroidManifest.xml.
		int id = item.getItemId();
		if (id == R.id.action_settings) {
			return true;
		}
		return super.onOptionsItemSelected(item);
	}




	@Override
	public boolean onMenuItemClick(MenuItem item) {
		BluetoothDevice currentDevice = mmDevices.get(item.getItemId());		
		
		Log.d("Device Name",currentDevice.getName());

		myLabel.setText("Connected");
		btStatus.setText("Connected (closed)");
		pairedDevice.setText(currentDevice.getName());
		mmDevice = currentDevice;


		return false;
	}
	
	private void stopMovement(){
		Log.d("Move Debug","move_up: "+Boolean.toString(move_up));
		move_up = false;
		move_left = false;
		move_right = false;
		move_down = false;
	}
	
	private void setImageListeners(){
		////////////////////////////////////////////////////////////////
		//
		//Arrow touch listeners
		//
		///////////////////////////////////////////////////////////////
		arrow_up.setOnTouchListener(new OnTouchListener()
        {

			@Override
            public boolean onTouch(View v, MotionEvent event)
            {
                switch(event.getAction()){
                	case MotionEvent.ACTION_DOWN:
                		myLabel.setText("event Down Up");
                		arrow_up.setImageResource(R.drawable.arrow_up_pressed);
                		stopMovement();
                		Log.d("Move Debug","move_up: "+Boolean.toString(move_up));
                		move_up = true;
                		break;
                	case MotionEvent.ACTION_MOVE:
                		myLabel.setText("event Move");
                		break;
                	case MotionEvent.ACTION_UP:
                		myLabel.setText("event Up Up");
                		arrow_up.setImageResource(R.drawable.arrow_up);
                		Log.d("Move Debug","move_up: "+Boolean.toString(move_up));
                		move_up = false;
                		break;
                	case MotionEvent.ACTION_OUTSIDE:
                		myLabel.setText("event Outside");
                		break;                	
                }
            	return true;
            }
        });
		
		arrow_left.setOnTouchListener(new OnTouchListener()
        {

			@Override
            public boolean onTouch(View v, MotionEvent event)
            {
				switch(event.getAction()){
                	case MotionEvent.ACTION_DOWN:
                		myLabel.setText("event Down left");
                		arrow_left.setImageResource(R.drawable.arrow_left_pressed);
                		stopMovement();
                		move_left = true;
                		break;
                	case MotionEvent.ACTION_MOVE:
                		myLabel.setText("event Move");
                		break;
                	case MotionEvent.ACTION_UP:
                		myLabel.setText("event Up Left");
                		arrow_left.setImageResource(R.drawable.arrow_left);
                		move_left = false;
                		break;
                	case MotionEvent.ACTION_OUTSIDE:
                		myLabel.setText("event Outside");
                		break;                	
                }
            	return true;
            }
        });
		
		arrow_right.setOnTouchListener(new OnTouchListener()
        {

			@Override
            public boolean onTouch(View v, MotionEvent event)
            {
                switch(event.getAction()){
                	case MotionEvent.ACTION_DOWN:
                		myLabel.setText("event Down Right");
                		arrow_right.setImageResource(R.drawable.arrow_right_pressed);
                		stopMovement();
                		move_right = true;
                		break;
                	case MotionEvent.ACTION_MOVE:
                		myLabel.setText("event Move");
                		break;
                	case MotionEvent.ACTION_UP:
                		myLabel.setText("event Up Right");
                		arrow_right.setImageResource(R.drawable.arrow_right);
                		move_right = false;
                		break;
                	case MotionEvent.ACTION_OUTSIDE:
                		myLabel.setText("event Outside");
                		break;                	
                }
            	return true;
            }
        });
		
		arrow_down.setOnTouchListener(new OnTouchListener()
        {

			@Override
            public boolean onTouch(View v, MotionEvent event)
            {
                switch(event.getAction()){
                	case MotionEvent.ACTION_DOWN:
                		myLabel.setText("event Down Down");
                		arrow_down.setImageResource(R.drawable.arrow_down_pressed);
                		stopMovement();
                		move_down = true;
                		break;
                	case MotionEvent.ACTION_MOVE:
                		myLabel.setText("event Move");
                		break;
                	case MotionEvent.ACTION_UP:
                		myLabel.setText("event Up Down");
                		arrow_down.setImageResource(R.drawable.arrow_down);
                		move_down = false;
                		break;
                	case MotionEvent.ACTION_OUTSIDE:
                		myLabel.setText("event Outside");
                		break;                	
                }
            	return true;
            }
        });
	}
	
	private void setButtonListeners(){
		////////////////////////////////////////////////////////////////
		//
		//Button listeners
		//
		///////////////////////////////////////////////////////////////
		mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
		if(!mBluetoothAdapter.isEnabled())
		{
			Intent enableBluetooth = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
			startActivityForResult(enableBluetooth, 0);
		}
		
		buttonConnect.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
            	connectBTDevice();                
            }
        });
		buttonOpen.setOnClickListener(new View.OnClickListener()
		{
			public void onClick(View v)
			{
				openBTConnection();                
			}
		});
		
		buttonClose.setOnClickListener(new View.OnClickListener()
		{
			public void onClick(View v)
			{
				closeBTConnection();                
			}
		});
		
		buttonChat.setOnClickListener(new View.OnClickListener()
		{
			public void onClick(View v)
			{
				sendData(); 
				editText.setText("", TextView.BufferType.EDITABLE);
			}
		});
	}
}
