package re.frida;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Looper;
import android.os.Process;
import android.util.Base64;
import android.util.Base64OutputStream;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.lang.Class;
import java.lang.Exception;
import java.lang.Object;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TimeZone;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class HelperBackend {
	private final Context mContext;
	private final PackageManager mPackageManager;
	private final ActivityManager mActivityManager;
	private final Object mContextWrapper;

	private Method mGetApplicationInfoAsUser;
	private final Method mForceStopPackage;
	private Method mForceStopPackageAsUser;
	private Method mUserHandleOf;
	private Method mCreatePackageContextAsUser;
	private Method mGetPackageInfoAsUser;
	private Object mActivityTaskManager;

	private Method mStartActivityAsUser;
	private Class<?>[] mStartActivityAsUserParamTypes;
	private int mStartActivityIntentIndex = -1;
	private int mStartActivityCallingPackageIndex = -1;
	private int mStartActivityUserIdIndex = -1;

	private Object mActivityManagerService;
	private Method mStartActivityLegacy;
	private Class<?>[] mStartActivityLegacyParamTypes;
	private int mLegacyStartIntentIndex = -1;
	private int mLegacyStartCallingPackageIndex = -1;
	private int mLegacyStartResolvedTypeIndex = -1;
	private int mLegacyStartRequestCodeIndex = -1;
	private int mLegacyStartUserIdIndex = -1;

	private Method mSendBroadcastAsUser;

	private boolean mMultiUserSupported;

	private final String mLauncherPkgName;
	private Field mSplitPublicSourceDirsField;

	private static final Pattern sStatusUidPattern = Pattern.compile("^Uid:\\s+\\d+\\s+(\\d+)\\s+\\d+\\s+\\d+$", Pattern.MULTILINE);
	private final long mSystemBootTime;
	private final long mMillisecondsPerJiffy;

	private final SimpleDateFormat mIso8601;

	private Method mGetpwuid;
	private Object mGetpwuidReceiver;
	private Field mPwnameField;

	private Method mReadlink;
	private Object mReadlinkReceiver;

	public HelperBackend() {
		Looper.prepareMainLooper();

		preInitVendorExtensions();

		Context context;
		try {
			Class<?> ActivityThread = Class.forName("android.app.ActivityThread");
			Object activityThread = ActivityThread.getDeclaredMethod("systemMain").invoke(null);
			mContext = (Context) ActivityThread.getDeclaredMethod("getSystemContext").invoke(activityThread);
		} catch (InvocationTargetException e) {
			throw new RuntimeException(e.getCause());
		} catch (Throwable e) {
			throw new RuntimeException(e);
		}
		mPackageManager = mContext.getPackageManager();
		mActivityManager = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);

		Class<?> ContextWrapper;
		try {
			ContextWrapper = Class.forName("android.content.ContextWrapper");
			Constructor<?> mContextWrapperCtor = ContextWrapper.getConstructor(Context.class);
			mContextWrapper = mContextWrapperCtor.newInstance(mContext);
		} catch (Throwable e) {
			throw new RuntimeException(e);
		}

		if (!tryInitActivityStartApi())
			initLegacyActivityStartApi();

		try {
			mGetApplicationInfoAsUser = PackageManager.class.getDeclaredMethod("getApplicationInfoAsUser", String.class,
					int.class, int.class);
			mGetPackageInfoAsUser = PackageManager.class.getDeclaredMethod("getPackageInfoAsUser", String.class, int.class,
					int.class);

			mForceStopPackageAsUser = ActivityManager.class.getDeclaredMethod("forceStopPackageAsUser", String.class,
					int.class);

			Class<?> UserHandle = Class.forName("android.os.UserHandle");
			mUserHandleOf = UserHandle.getDeclaredMethod("of", int.class);

			mCreatePackageContextAsUser = Context.class.getDeclaredMethod("createPackageContextAsUser", String.class, int.class,
					UserHandle);

			mSendBroadcastAsUser = ContextWrapper.getDeclaredMethod("sendBroadcastAsUser", Intent.class, UserHandle);

			mMultiUserSupported = true;
		} catch (Exception e) {
		}
		try {
			mForceStopPackage = ActivityManager.class.getDeclaredMethod("forceStopPackage", String.class);
		} catch (NoSuchMethodException e) {
			throw new RuntimeException(e);
		}

		mLauncherPkgName = detectLauncherPackageName();
		try {
			mSplitPublicSourceDirsField = ApplicationInfo.class.getField("splitPublicSourceDirs");
		} catch (Throwable t) {
		}

		mSystemBootTime = querySystemBootTime();
		mMillisecondsPerJiffy = 1000 / getClockTicksPerSecond();

		mIso8601 = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US);
		mIso8601.setTimeZone(TimeZone.getTimeZone("UTC"));
		try {
			mGetpwuid = Class.forName("android.system.Os").getDeclaredMethod("getpwuid", int.class);
			mGetpwuidReceiver = null;
			mPwnameField = Class.forName("android.system.StructPasswd").getDeclaredField("pw_name");
		} catch (Exception e1) {
			try {
				Class<?> libcore = Class.forName("libcore.io.Libcore");
				Object os = libcore.getField("os").get(null);
				Class<?> osClass = Class.forName("libcore.io.Os");
				mGetpwuid = osClass.getDeclaredMethod("getpwuid", int.class);
				mGetpwuidReceiver = os;
				mPwnameField = Class.forName("libcore.io.StructPasswd").getDeclaredField("pw_name");
			} catch (Exception e2) {
				throw new RuntimeException("getpwuid is not available on this device/API level");
			}
		}

		try {
			mReadlink = Class.forName("android.system.Os").getDeclaredMethod("readlink", String.class);
			mReadlinkReceiver = null;
		} catch (Exception e1) {
			try {
				mReadlink = java.io.File.class.getDeclaredMethod("readlink", String.class);
				mReadlink.setAccessible(true);
				mReadlinkReceiver = null;
			} catch (Exception e2) {
				throw new RuntimeException("readlink is not available on this device/API level: " + e2.toString());
			}
		}
	}

	private static void preInitVendorExtensions() {
		// Nothing OS: AppOpsManager.<clinit> chains into NtAppOpsManager.<clinit>, which
		// calls NtExtFactory.getOrCreate(...). The factory dispatches through a static
		// INtExtFactory singleton populated by NtExtFactory.init() — normally invoked
		// from RuntimeInit.commonInit() during process bring-up, which our helper
		// bypasses. Invoke it ourselves so the static initializer chain doesn't NPE.
		invokeStaticVoidIfPresent("com.nothing.NtExtFactory", "init");
	}

	private static void invokeStaticVoidIfPresent(String className, String methodName) {
		try {
			Class.forName(className).getMethod(methodName).invoke(null);
		} catch (ClassNotFoundException e) {
		} catch (Throwable ignored) {
		}
	}

	private boolean tryInitActivityStartApi() {
		try {
			Class<?> ActivityTaskManager = Class.forName("android.app.ActivityTaskManager");
			Method getService = ActivityTaskManager.getDeclaredMethod("getService");
			mActivityTaskManager = getService.invoke(null);

			Method best = null;
			int intentIndex = -1;
			int callingPkgIndex = -1;
			int userIdIndex = -1;

			for (Method m : mActivityTaskManager.getClass().getMethods()) {
				if (!m.getName().equals("startActivityAsUser"))
					continue;

				Class<?>[] p = m.getParameterTypes();

				int iIntent = indexOf(p, Intent.class);
				if (iIntent == -1)
					continue;

				int iCalling = indexOfFirst(p, String.class);
				if (iCalling == -1)
					continue;

				int iUserId = indexOfLast(p, int.class);
				if (iUserId == -1)
					continue;

				best = m;
				intentIndex = iIntent;
				callingPkgIndex = iCalling;
				userIdIndex = iUserId;
				break;
			}

			if (best == null)
				throw new NoSuchMethodException("No compatible startActivityAsUser overload found");

			mStartActivityAsUser = best;
			mStartActivityAsUserParamTypes = best.getParameterTypes();
			mStartActivityIntentIndex = intentIndex;
			mStartActivityCallingPackageIndex = callingPkgIndex;
			mStartActivityUserIdIndex = userIdIndex;

			return true;
		} catch (Exception e) {
			return false;
		}
	}

	private void initLegacyActivityStartApi() {
		try {
			Class<?> ActivityManagerNative = Class.forName("android.app.ActivityManagerNative");
			Method getDefault = ActivityManagerNative.getDeclaredMethod("getDefault");
			mActivityManagerService = getDefault.invoke(null);

			Method best = null;
			int intentIndex = -1;
			int callingPkgIndex = -1;
			int resolvedTypeIndex = -1;
			int requestCodeIndex = -1;
			int userIdIndex = -1;

			for (String name : new String[] { "startActivityAsUser", "startActivity" }) {
				for (Method m : mActivityManagerService.getClass().getMethods()) {
					if (!m.getName().equals(name))
						continue;

					Class<?>[] p = m.getParameterTypes();

					int iIntent = indexOf(p, Intent.class);
					if (iIntent == -1)
						continue;

					int iCalling = indexOfFirst(p, String.class);
					if (iCalling == -1)
						continue;

					int iResolvedType = -1;
					for (int i = iIntent + 1; i != p.length; i++) {
						if (p[i] == String.class) {
							iResolvedType = i;
							break;
						}
					}
					if (iResolvedType == -1)
						continue;

					int iRequestCode = indexOfFirst(p, int.class);
					if (iRequestCode == -1)
						continue;

					int iUserId = name.equals("startActivityAsUser") ? indexOfLast(p, int.class) : -1;

					best = m;
					intentIndex = iIntent;
					callingPkgIndex = iCalling;
					resolvedTypeIndex = iResolvedType;
					requestCodeIndex = iRequestCode;
					userIdIndex = iUserId;
					break;
				}

				if (best != null)
					break;
			}

			if (best == null)
				throw new NoSuchMethodException("No compatible legacy startActivity overload found");

			mStartActivityLegacy = best;
			mStartActivityLegacyParamTypes = best.getParameterTypes();
			mLegacyStartIntentIndex = intentIndex;
			mLegacyStartCallingPackageIndex = callingPkgIndex;
			mLegacyStartResolvedTypeIndex = resolvedTypeIndex;
			mLegacyStartRequestCodeIndex = requestCodeIndex;
			mLegacyStartUserIdIndex = userIdIndex;
		} catch (Exception e) {
		}
	}

	public String handleRequestString(String requestJson) throws JSONException {
		JSONArray request = new JSONArray(requestJson);
		JSONArray result = handleRequest(request);
		return (result != null) ? result.toString() : null;
	}

	public JSONArray handleRequest(JSONArray request) throws JSONException {
		String type = request.getString(0);

		if (type.equals("get-frontmost-application"))
			return getFrontmostApplication(request);

		if (type.equals("enumerate-applications"))
			return enumerateApplications(request);

		if (type.equals("enumerate-processes"))
			return enumerateProcesses(request);

		if (type.equals("get-process-name"))
			return getProcessName(request);

		if (type.equals("start-activity"))
			return startActivity(request);

		if (type.equals("send-broadcast"))
			return sendBroadcast(request);

		if (type.equals("stop-package"))
			return stopPackage(request);

		if (type.equals("try-stop-package-by-pid"))
			return tryStopPackageByPid(request);

		throw new UnsupportedOperationException("Unsupported request type");
	}

	private JSONArray getFrontmostApplication(JSONArray request) throws JSONException {
		Scope scope = Scope.valueOf(request.getString(1).toUpperCase());

		String pkgName = getFrontmostPackageName();
		if (pkgName == null)
			return null;

		ApplicationInfo appInfo;
		try {
			appInfo = mPackageManager.getApplicationInfo(pkgName, 0);
		} catch (NameNotFoundException e) {
			return null;
		}

		CharSequence appLabel = appInfo.loadLabel(mPackageManager);

		int pid = 0;
		List<RunningAppProcessInfo> pkgProcesses = getAppProcesses().get(pkgName);
		if (pkgProcesses != null) {
			pid = pkgProcesses.get(0).pid;
		}

		JSONObject parameters = null;
		if (scope != Scope.MINIMAL) {
			try {
				parameters = fetchAppParameters(appInfo, scope);
			} catch (NameNotFoundException e) {
				return null;
			}

			if (pid != 0) {
				try {
					addProcessMetadata(parameters, pid);
				} catch (IOException e) {
					return null;
				}
			}
		}

		JSONArray app = new JSONArray();
		app.put(pkgName);
		app.put(appLabel);
		app.put(pid);
		app.put(parameters);

		return app;
	}

	private JSONArray enumerateApplications(JSONArray request) throws JSONException {
		JSONArray identifiersValue = request.getJSONArray(1);
		Scope scope = Scope.valueOf(request.getString(2).toUpperCase());

		List<ApplicationInfo> apps;
		int numIdentifiers = identifiersValue.length();
		if (numIdentifiers > 0) {
			apps = new ArrayList<ApplicationInfo>();
			for (int i = 0; i != numIdentifiers; i++) {
				String pkgName = identifiersValue.getString(i);
				try {
					apps.add(mPackageManager.getApplicationInfo(pkgName, 0));
				} catch (NameNotFoundException e) {
				}
			}
		} else {
			apps = getLauncherApplications();
		}

		JSONArray result = new JSONArray();

		Map<String, List<RunningAppProcessInfo>> processes = getAppProcesses();
		String frontmostPkgName = (scope != Scope.MINIMAL) ? getFrontmostPackageName() : null;

		for (ApplicationInfo appInfo : apps) {
			String pkgName = appInfo.packageName;

			CharSequence appLabel = appInfo.loadLabel(mPackageManager);

			int pid = 0;
			List<RunningAppProcessInfo> pkgProcesses = processes.get(pkgName);
			if (pkgProcesses != null) {
				pid = pkgProcesses.get(0).pid;
			}

			JSONObject parameters = null;
			if (scope != Scope.MINIMAL) {
				try {
					parameters = fetchAppParameters(appInfo, scope);
				} catch (NameNotFoundException e) {
					continue;
				}

				if (pid != 0) {
					try {
						addProcessMetadata(parameters, pid);
					} catch (IOException e) {
						pid = 0;
					}
				}

				if (pid != 0 && pkgName.equals(frontmostPkgName)) {
					parameters.put("frontmost", true);
				}
			}

			JSONArray app = new JSONArray();
			app.put(pkgName);
			app.put(appLabel);
			app.put(pid);
			app.put(parameters);

			result.put(app);
		}

		return result;
	}

	private JSONArray enumerateProcesses(JSONArray request) throws JSONException {
		JSONArray pidsValue = request.getJSONArray(1);
		Scope scope = Scope.valueOf(request.getString(2).toUpperCase());

		int numPids = pidsValue.length();
		List<Integer> pids = new ArrayList<Integer>(numPids);
		if (numPids > 0) {
			for (int i = 0; i != numPids; i++) {
				pids.add(pidsValue.getInt(i));
			}
		} else {
			int myPid = Process.myPid();

			for (File candidate : new File("/proc").listFiles()) {
				if (!candidate.isDirectory()) {
					continue;
				}

				int pid;
				try {
					pid = Integer.parseInt(candidate.getName());
				} catch (NumberFormatException e) {
					continue;
				}

				if (pid == myPid) {
					continue;
				}

				pids.add(pid);
			}
		}

		JSONArray result = new JSONArray();

		Map<String, List<RunningAppProcessInfo>> appProcessByPkgName = getAppProcesses();

		Map<Integer, RunningAppProcessInfo> appProcessByPid = new HashMap<Integer, RunningAppProcessInfo>();
		for (List<RunningAppProcessInfo> processes : appProcessByPkgName.values()) {
			for (RunningAppProcessInfo process : processes) {
				appProcessByPid.put(process.pid, process);
			}
		}

		Map<String, ApplicationInfo> appInfoByPkgName = new HashMap<String, ApplicationInfo>();
		for (ApplicationInfo appInfo : getLauncherApplications()) {
			appInfoByPkgName.put(appInfo.packageName, appInfo);
		}

		Map<Integer, ApplicationInfo> appInfoByPid = new HashMap<Integer, ApplicationInfo>();
		for (List<RunningAppProcessInfo> processes : appProcessByPkgName.values()) {
			RunningAppProcessInfo mostImportantProcess = processes.get(0);
			for (String pkgName : mostImportantProcess.pkgList) {
				ApplicationInfo appInfo = appInfoByPkgName.get(pkgName);
				if (appInfo != null) {
					appInfoByPid.put(mostImportantProcess.pid, appInfo);
					break;
				}
			}
		}

		int frontmostPid = -1;
		if (scope != Scope.MINIMAL) {
			String frontmostPkgName = getFrontmostPackageName();
			if (frontmostPkgName != null) {
				List<RunningAppProcessInfo> frontmostProcesses = appProcessByPkgName.get(frontmostPkgName);
				if (frontmostProcesses != null) {
					frontmostPid = frontmostProcesses.get(0).pid;
				}
			}
		}

		for (Integer pid : pids) {
			File procDir = new File("/proc", pid.toString());

			ApplicationInfo appInfo = appInfoByPid.get(pid);

			CharSequence name;
			if (appInfo != null) {
				name = appInfo.loadLabel(mPackageManager);
			} else {
				String cmdline;
				try {
					cmdline = getFileContentsAsString(new File(procDir, "cmdline"));
				} catch (IOException e) {
					continue;
				}

				boolean isKernelProcess = cmdline.isEmpty();
				if (isKernelProcess) {
					continue;
				}

				name = deriveProcessNameFromCmdline(cmdline);
			}

			JSONObject parameters = null;
			if (scope != Scope.MINIMAL) {
				parameters = new JSONObject();

				try {
					String exe = readlink(new File(procDir, "exe").getAbsolutePath());
					if (exe != null) {
						File program = new File(exe);
						parameters.put("path", program.getAbsolutePath());
					}
				} catch (Exception e) {
				}

				try {
					addProcessMetadata(parameters, pid);
				} catch (IOException e) {
					continue;
				}

				RunningAppProcessInfo appProcess = appProcessByPid.get(pid);
				if (appProcess != null) {
					JSONArray ids = new JSONArray();
					for (String pkgName : appProcess.pkgList) {
						ids.put(pkgName);
					}
					parameters.put("applications", ids);
				}

				if (scope == Scope.FULL && appInfo != null) {
					JSONArray icons = new JSONArray();
					icons.put(fetchAppIcon(appInfo));
					parameters.put("$icons", icons);
				}

				if (pid == frontmostPid) {
					parameters.put("frontmost", true);
				}
			}

			JSONArray process = new JSONArray();
			process.put(pid);
			process.put(name);
			process.put(parameters);

			result.put(process);
		}

		return result;
	}

	private JSONArray getProcessName(JSONArray request) throws JSONException {
		String pkgName = request.getString(1);
		int uid = request.getInt(2);

		try {
			ApplicationInfo appInfo = getApplicationInfoForUser(pkgName, uid);
			return ok(appInfo.processName);
		} catch (NameNotFoundException e) {
			return error(
					"INVALID_ARGUMENT",
					"Unable to find application with identifier '" + pkgName + "'" +
					((uid != 0) ? " belonging to uid " + uid : ""));
		} catch (Throwable e) {
			return error("NOT_SUPPORTED", e.toString());
		}
	}

	private JSONArray startActivity(JSONArray request) throws JSONException {
		String pkgName = request.getString(1);
		String activity = request.isNull(2) ? null : request.getString(2);
		int uid = request.getInt(3);

		try {
			getApplicationInfoForUser(pkgName, uid);

			Context ctx = mContext;
			PackageManager pm = mPackageManager;
			Object user = null;

			if (uid != 0) {
				user = userHandleOf(uid);
				ctx = (Context) mCreatePackageContextAsUser.invoke(mContext, pkgName, 0, user);
				pm = ctx.getPackageManager();
			}

			Intent intent = pm.getLaunchIntentForPackage(pkgName);
			if (intent == null && activity == null) {
				return error("INVALID_ARGUMENT", "Unable to find a front-door activity");
			}

			if (intent == null) {
				intent = new Intent();
			}

			intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

			if (activity != null) {
				PackageInfo pkgInfo = getPackageInfoForUser(pkgName, PackageManager.GET_ACTIVITIES, uid);

				boolean found = false;
				if (pkgInfo.activities != null) {
					for (android.content.pm.ActivityInfo ai : pkgInfo.activities) {
						if (activity.equals(ai.name)) {
							found = true;
							break;
						}
					}
				}

				if (!found)
					return error("INVALID_ARGUMENT", "Unable to find activity with identifier '" + activity + "'");

				intent.setClassName(pkgName, activity);
			}

			if (mStartActivityAsUser != null)
				startActivityViaAtm(intent, uid, pkgName);
			else
				startActivityViaAm(intent, uid, pkgName);

			return okVoid();
		} catch (NameNotFoundException e) {
			return error("INVALID_ARGUMENT",
					"Unable to find application with identifier '" + pkgName + "'" +
					((uid != 0) ? " belonging to uid " + uid : ""));
		} catch (Throwable e) {
			return error("NOT_SUPPORTED", e.toString());
		}
	}

	private void startActivityViaAtm(Intent intent, int uid, String callingPackage) throws Exception {
		if (mStartActivityAsUser == null)
			throw new UnsupportedOperationException("startActivityAsUser unavailable");

		Object[] args = new Object[mStartActivityAsUserParamTypes.length];

		for (int i = 0; i != args.length; i++) {
			if (mStartActivityAsUserParamTypes[i] == int.class)
				args[i] = 0;
			else
				args[i] = null;
		}

		args[mStartActivityIntentIndex] = intent;
		args[mStartActivityCallingPackageIndex] = callingPackage;
		args[mStartActivityUserIdIndex] = (uid != 0) ? uid : 0;

		mStartActivityAsUser.invoke(mActivityTaskManager, args);
	}

	private void startActivityViaAm(Intent intent, int uid, String callingPackage) throws Exception {
		if (mStartActivityLegacy == null)
			throw new UnsupportedOperationException("legacy startActivity unavailable");

		Object[] args = new Object[mStartActivityLegacyParamTypes.length];

		for (int i = 0; i != args.length; i++) {
			if (mStartActivityLegacyParamTypes[i] == int.class)
				args[i] = 0;
			else
				args[i] = null;
		}

		args[mLegacyStartIntentIndex] = intent;
		args[mLegacyStartCallingPackageIndex] = callingPackage;
		args[mLegacyStartResolvedTypeIndex] = intent.resolveTypeIfNeeded(mContext.getContentResolver());
		args[mLegacyStartRequestCodeIndex] = -1;

		if (mLegacyStartUserIdIndex != -1)
			args[mLegacyStartUserIdIndex] = (uid != 0) ? uid : 0;

		mStartActivityLegacy.invoke(mActivityManagerService, args);
	}

	private JSONArray sendBroadcast(JSONArray request) throws JSONException {
		String pkgName = request.getString(1);
		String receiver = request.getString(2);
		String action = request.getString(3);
		int uid = request.getInt(4);

		try {
			getApplicationInfoForUser(pkgName, uid);

			Intent intent = new Intent();
			intent.setComponent(new ComponentName(pkgName, receiver));
			intent.setAction(action);

			if (uid != 0) {
				Object user = userHandleOf(uid);
				mSendBroadcastAsUser.invoke(mContextWrapper, intent, user);
			} else {
				mContext.sendBroadcast(intent);
			}

			return okVoid();
		} catch (NameNotFoundException e) {
			return error("INVALID_ARGUMENT",
					"Unable to find application with identifier '" + pkgName + "'" +
					((uid != 0) ? " belonging to uid " + uid : ""));
		} catch (Throwable e) {
			return error("NOT_SUPPORTED", e.toString());
		}
	}

	private JSONArray stopPackage(JSONArray request) throws JSONException {
		String pkgName = request.getString(1);
		int uid = request.getInt(2);

		try {
			getApplicationInfoForUser(pkgName, uid);

			try {
				if (uid != 0) {
					mForceStopPackageAsUser.invoke(mActivityManager, pkgName, uid);
				} else {
					mForceStopPackage.invoke(mActivityManager, pkgName);
				}
			} catch (InvocationTargetException e) {
				throw e.getCause();
			}

			return okVoid();
		} catch (NameNotFoundException e) {
			return error(
					"INVALID_ARGUMENT",
					"Unable to find application with identifier '" + pkgName + "'" +
					((uid != 0) ? " belonging to uid " + uid : ""));
		} catch (Throwable e) {
			return error("NOT_SUPPORTED", e.toString());
		}
	}

	private JSONArray tryStopPackageByPid(JSONArray request) throws JSONException {
		int pid = request.getInt(1);

		try {
			List<RunningAppProcessInfo> processes = mActivityManager.getRunningAppProcesses();

			for (RunningAppProcessInfo process : processes) {
				if (process.pid != pid)
					continue;

				for (String pkgName : process.pkgList) {
					try {
						mForceStopPackage.invoke(mActivityManager, pkgName);
					} catch (InvocationTargetException e) {
						throw e.getCause();
					}
				}

				return okBoolean(true);
			}

			return okBoolean(false);
		} catch (Throwable e) {
			return error("NOT_SUPPORTED", e.toString());
		}
	}

	private static JSONArray ok(Object value) throws JSONException {
		JSONArray r = new JSONArray();
		r.put("ok");
		r.put(value);
		return r;
	}

	private static JSONArray okVoid() throws JSONException {
		JSONArray r = new JSONArray();
		r.put("ok");
		return r;
	}

	private static JSONArray okBoolean(boolean value) throws JSONException {
		JSONArray r = new JSONArray();
		r.put("ok");
		r.put(value);
		return r;
	}

	private static JSONArray error(String code, String message) throws JSONException {
		JSONArray r = new JSONArray();
		r.put("error");
		r.put(code);
		r.put(message);
		return r;
	}

	private String getFrontmostPackageName() {
		List<ActivityManager.RunningTaskInfo> tasks = mActivityManager.getRunningTasks(1);
		if (tasks.isEmpty()) {
			return null;
		}

		ActivityManager.RunningTaskInfo task = tasks.get(0);
		ComponentName name = task.topActivity;
		if (name == null) {
			return null;
		}

		String pkgName = name.getPackageName();
		if (pkgName == null || pkgName.equals(mLauncherPkgName)) {
			return null;
		}

		return pkgName;
	}

	private List<ApplicationInfo> getLauncherApplications() {
		List<ApplicationInfo> apps = new ArrayList<ApplicationInfo>();

		Intent intent = new Intent(Intent.ACTION_MAIN);
		intent.addCategory(Intent.CATEGORY_LAUNCHER);

		for (ResolveInfo resolveInfo : mPackageManager.queryIntentActivities(intent, 0)) {
			apps.add(resolveInfo.activityInfo.applicationInfo);
		}

		return apps;
	}

	private JSONObject fetchAppParameters(ApplicationInfo appInfo, Scope scope) throws NameNotFoundException {
		JSONObject parameters = new JSONObject();

		PackageInfo packageInfo = mPackageManager.getPackageInfo(appInfo.packageName, 0);

		try {
			parameters.put("version", packageInfo.versionName);
			parameters.put("build", Integer.toString(packageInfo.versionCode));
			parameters.put("sources", fetchAppSources(appInfo));
			parameters.put("data-dir", appInfo.dataDir);
			parameters.put("target-sdk", appInfo.targetSdkVersion);
			if ((appInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
				parameters.put("debuggable", true);
			}

			if (scope == Scope.FULL) {
				JSONArray icons = new JSONArray();
				icons.put(fetchAppIcon(appInfo));
				parameters.put("$icons", icons);
			}
		} catch (JSONException e) {
			throw new RuntimeException(e);
		}

		return parameters;
	}

	private JSONArray fetchAppSources(ApplicationInfo appInfo) {
		JSONArray sources = new JSONArray();
		sources.put(appInfo.publicSourceDir);

		if (mSplitPublicSourceDirsField != null) {
			try {
				String[] splitDirs = (String[]) mSplitPublicSourceDirsField.get(appInfo);
				if (splitDirs != null) {
					for (String splitDir : splitDirs) {
						sources.put(splitDir);
					}
				}
			} catch (Throwable t) {
			}
		}

		return sources;
	}

	private String fetchAppIcon(ApplicationInfo appInfo) {
		Drawable icon = mPackageManager.getApplicationIcon(appInfo);

		int width = icon.getIntrinsicWidth();
		int height = icon.getIntrinsicHeight();

		Bitmap bitmap = Bitmap.createBitmap(width, height, Config.ARGB_8888);
		Canvas canvas = new Canvas(bitmap);
		icon.setBounds(0, 0, width, height);
		icon.draw(canvas);

		ByteArrayOutputStream output = new ByteArrayOutputStream();
		bitmap.compress(CompressFormat.PNG, 100, new Base64OutputStream(output, Base64.NO_WRAP));

		return output.toString();
	}

	private Map<String, List<RunningAppProcessInfo>> getAppProcesses() {
		Map<String, List<RunningAppProcessInfo>> processes = new HashMap<String, List<RunningAppProcessInfo>>();

		for (RunningAppProcessInfo processInfo : mActivityManager.getRunningAppProcesses()) {
			for (String pkgName : processInfo.pkgList) {
				List<RunningAppProcessInfo> entries = processes.get(pkgName);
				if (entries == null) {
					entries = new ArrayList<RunningAppProcessInfo>();
					processes.put(pkgName, entries);
				}
				entries.add(processInfo);
				if (entries.size() > 1) {
					Collections.sort(entries, new Comparator<RunningAppProcessInfo>() {
						@Override
						public int compare(RunningAppProcessInfo a, RunningAppProcessInfo b) {
							return a.importance - b.importance;
						}
					});
				}
			}
		}

		return processes;
	}

	private ApplicationInfo getApplicationInfoForUser(String pkgName, int uid) throws NameNotFoundException {
		checkUidOptionSupported(uid);

		if (uid == 0) {
			return mPackageManager.getApplicationInfo(pkgName, 0);
		}

		if (!mMultiUserSupported) {
			throw new RuntimeException("uid option not supported");
		}

		try {
			return (ApplicationInfo) mGetApplicationInfoAsUser.invoke(mPackageManager, pkgName, 0, uid);
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();
			if (cause instanceof NameNotFoundException)
				throw (NameNotFoundException) cause;
			throw new RuntimeException(cause);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		}
	}

	private PackageInfo getPackageInfoForUser(String pkgName, int flags, int uid) throws NameNotFoundException {
		checkUidOptionSupported(uid);

		if (uid == 0) {
			return mPackageManager.getPackageInfo(pkgName, flags);
		}

		try {
			return (PackageInfo) mGetPackageInfoAsUser.invoke(mPackageManager, pkgName, flags, uid);
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();
			if (cause instanceof NameNotFoundException)
				throw (NameNotFoundException) cause;
			throw new RuntimeException(cause);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		}
	}

	private void checkUidOptionSupported(int uid) {
		if (uid != 0 && !mMultiUserSupported) {
			throw new UnsupportedOperationException("The “uid” option is not supported on the current Android OS version");
		}
	}

	private Object userHandleOf(int uid) throws Exception {
		return mUserHandleOf.invoke(null, uid);
	}

	private static String deriveProcessNameFromCmdline(String cmdline) {
		String str = cmdline;
		int spaceDashOffset = str.indexOf(" -");
		if (spaceDashOffset != -1) {
			str = str.substring(0, spaceDashOffset);
		}
		return new File(str).getName();
	}

	private void addProcessMetadata(JSONObject parameters, int pid) throws IOException {
		File procNode = new File("/proc/" + Integer.toString(pid));

		String status = getFileContentsAsString(new File(procNode, "status"));
		Matcher m = sStatusUidPattern.matcher(status);
		m.find();
		int uid = Integer.parseInt(m.group(1));

		String stat = getFileContentsAsString(new File(procNode, "stat"));
		int commFieldEndOffset = stat.indexOf(')');
		int stateFieldStartOffset = commFieldEndOffset + 2;
		String[] statFields = stat.substring(stateFieldStartOffset).split(" ");
		int manPageFieldIdOffset = 3;

		int ppid = Integer.parseInt(statFields[4 - manPageFieldIdOffset]);

		long startTimeDeltaInJiffies = Long.parseLong(statFields[22 - manPageFieldIdOffset]);
		long startTimeDeltaInMilliseconds = startTimeDeltaInJiffies * mMillisecondsPerJiffy;
		Date started = new Date(mSystemBootTime + startTimeDeltaInMilliseconds);

		try {
			parameters.put("user", resolveUserIdToName(uid));
			parameters.put("ppid", ppid);
			parameters.put("started", mIso8601.format(started));
		} catch (JSONException e) {
			throw new RuntimeException(e);
		}
	}

	private static long querySystemBootTime() {
		String stat;
		try {
			stat = getFileContentsAsString(new File("/proc/stat"));
		} catch (IOException e) {
			throw new RuntimeException(e);
		}

		Matcher m = Pattern.compile("^btime (\\d+)$", Pattern.MULTILINE).matcher(stat);
		m.find();
		return Long.parseLong(m.group(1)) * 1000;
	}

	private static long getClockTicksPerSecond() {
		try {
			Class<?> osClass = Class.forName("android.system.Os");
			Class<?> osConstants = Class.forName("android.system.OsConstants");
			int key = osConstants.getField("_SC_CLK_TCK").getInt(null);
			Method sysconf = osClass.getMethod("sysconf", int.class);
			return ((Long) sysconf.invoke(null, key)).longValue();
		} catch (Throwable t1) {
		}

		try {
			Class<?> libcore = Class.forName("libcore.io.Libcore");
			Object os = libcore.getField("os").get(null);
			Class<?> osClass = Class.forName("libcore.io.Os");
			Class<?> osConstants = Class.forName("libcore.io.OsConstants");
			int key = osConstants.getField("_SC_CLK_TCK").getInt(null);
			Method sysconf = osClass.getMethod("sysconf", int.class);
			return ((Long) sysconf.invoke(os, key)).longValue();
		} catch (Throwable t2) {
			throw new RuntimeException("sysconf(_SC_CLK_TCK) is not available on this device/API level");
		}
	}

	private String readlink(String path) throws Exception {
		try {
			return (String) mReadlink.invoke(mReadlinkReceiver, path);
		} catch (InvocationTargetException e) {
			throw (Exception) e.getCause();
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		}
	}

	private String resolveUserIdToName(int uid) {
		try {
			return (String) mPwnameField.get(mGetpwuid.invoke(mGetpwuidReceiver, uid));
		} catch (IllegalArgumentException | IllegalAccessException | InvocationTargetException e) {
			throw new RuntimeException(e);
		}
	}

	private String detectLauncherPackageName() {
		Intent intent = new Intent(Intent.ACTION_MAIN);
		intent.addCategory(Intent.CATEGORY_HOME);

		List<ResolveInfo> launchers = mPackageManager.queryIntentActivities(intent, 0);
		if (launchers.isEmpty()) {
			return null;
		}

		return launchers.get(0).activityInfo.packageName;
	}

	private static String getFileContentsAsString(File file) throws IOException {
		ByteArrayOutputStream result = new ByteArrayOutputStream();

		FileInputStream input = new FileInputStream(file);
		try {
			byte[] buffer = new byte[64 * 1024];
			while (true) {
				int n = input.read(buffer);
				if (n == -1) {
					break;
				}
				result.write(buffer, 0, n);
			}
		} finally {
			input.close();
		}

		return result.toString();
	}

	private static int indexOf(Class<?>[] p, Class<?> type) {
		for (int i = 0; i != p.length; i++) {
			if (p[i] == type)
				return i;
		}
		return -1;
	}

	private static int indexOfFirst(Class<?>[] p, Class<?> type) {
		return indexOf(p, type);
	}

	private static int indexOfLast(Class<?>[] p, Class<?> type) {
		for (int i = p.length - 1; i >= 0; i--) {
			if (p[i] == type)
				return i;
		}
		return -1;
	}
}

enum Scope {
	MINIMAL,
	METADATA,
	FULL;
}
