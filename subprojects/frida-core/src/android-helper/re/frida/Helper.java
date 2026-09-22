package re.frida;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.os.Looper;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.IOException;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class Helper {
	public static void main(String[] args) {
		if (args.length != 1) {
			System.err.println("Usage: frida-helper <instance-id>");
			System.exit(1);
			return;
		}

		String instanceId = args[0];

		new File("/data/local/tmp/frida-helper-" + instanceId + ".dex").delete();

		LocalServerSocket socket;
		try {
			socket = new LocalServerSocket("/frida-helper-" + instanceId);
		} catch (IOException e) {
			System.err.println(e);
			System.exit(2);
			return;
		}

		new Helper(socket).run();
	}

	private final HelperBackend mBackend;
	private final LocalServerSocket mSocket;
	private final Thread mWorker;

	private final int MAX_REQUEST_SIZE = 128 * 1024;

	public Helper(LocalServerSocket socket) {
		mBackend = new HelperBackend();
		mSocket = socket;
		mWorker = new Thread("Connection Listener") {
			public void run() {
				handleIncomingConnections();
			}
		};
	}

	private void run() {
		mWorker.start();
		Looper.loop();
	}

	private void handleIncomingConnections() {
		System.out.println("READY.");

		while (true) {
			try {
				LocalSocket client = mSocket.accept();
				Thread handler = new Thread("Connection Handler") {
					public void run() {
						handleConnection(client);
					}
				};
				handler.start();
			} catch (IOException e) {
				break;
			}
		}
	}

	protected void handleConnection(LocalSocket client) {
		DataInputStream input;
		DataOutputStream output;
		try {
			input = new DataInputStream(new BufferedInputStream(client.getInputStream()));
			output = new DataOutputStream(new BufferedOutputStream(client.getOutputStream()));
		} catch (IOException e) {
			return;
		}

		while (true) {
			try {
				int requestSize = input.readInt();
				if (requestSize < 1 || requestSize > MAX_REQUEST_SIZE)
					break;

				byte[] rawRequest = new byte[requestSize];
				input.readFully(rawRequest);

				JSONArray request = new JSONArray(new String(rawRequest));

				JSONArray response = mBackend.handleRequest(request);

				byte[] rawResponse = (response != null)
						? response.toString().getBytes()
						: JSONObject.NULL.toString().getBytes();
				output.writeInt(rawResponse.length);
				output.write(rawResponse);
				output.flush();
			} catch (JSONException e) {
				break;
			} catch (EOFException e) {
				break;
			} catch (IOException e) {
				break;
			}
		}

		try {
			client.close();
		} catch (IOException e) {
		}
	}
}
