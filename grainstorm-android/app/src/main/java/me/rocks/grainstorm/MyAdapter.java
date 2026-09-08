package me.rocks.grainstorm;

/**
 * Created by pr on 14.10.17.
 */

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.text.format.DateUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Objects;

import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.documentfile.provider.DocumentFile;
import androidx.recyclerview.widget.RecyclerView;

import org.jetbrains.annotations.NotNull;

import static me.rocks.grainstorm.MyApplication.read_header;
import static me.rocks.grainstorm.MyApplication.read_midiheader;
import static me.rocks.grainstorm.MyApplication.save_midimapping_callback;
import static me.rocks.grainstorm.PresetActivity.SAVE_MAPPING;
import static me.rocks.grainstorm.PresetActivity.SAVE_PRESET;
import static me.rocks.grainstorm.PresetActivity.SAVE_PROJECT;
import static me.rocks.grainstorm.PresetActivity.emptyString;


class MyAdapter extends RecyclerView.Adapter<MyAdapter.ViewHolder> {
    static final String[] paths = {"presets", "midimappings", "projects"};
    private final int type;
    private final static List<Presetitem> list0 = new ArrayList<>();
    private final static List<Presetitem> list1 = new ArrayList<>();
    private final static List<Presetitem> list2 = new ArrayList<>();
    private List<Presetitem> mList;

    static class ViewHolder extends RecyclerView.ViewHolder {
        View rootView;
        ImageView deleteButton;
        TextView name;
        TextView date;
        ViewHolder viewHolder;
        LinearLayout textLayout;
        ImageView infoButton;

        ViewHolder(View itemView) {
            super(itemView);
            viewHolder = this;
            rootView = itemView;
            deleteButton = itemView.findViewById(R.id.deletebutton);
            deleteButton.setTag(viewHolder);
            infoButton = itemView.findViewById(R.id.infobutton);
            infoButton.setTag(viewHolder);
            //counter = (AppCompatTextView) itemView.findViewById(R.id.counter);
            //counter.setTag(viewHolder);
            name = itemView.findViewById(R.id.presetname);
            name.setTag(this);
            date = itemView.findViewById(R.id.presetdate);
            date.setTag(this);
            rootView = itemView;
            itemView.setTag(this);
            textLayout = itemView.findViewById(R.id.textlayout);
            textLayout.setTag(this);
            //myLinearlayout = (TextView) itemView.findViewById(R.id.Linear);
            //recdate.setText()
            //iconTextView.setOnClickListener(this);
            //iconImageView.setOnLongClickListener(this);
        }
    }

    PresetActivity presetActivity;


    // Provide a suitable constructor (depends on the kind of dataset)
    MyAdapter(PresetActivity activity) {
        presetActivity = activity;
        type = presetActivity.type;
        if (type == 0)
            mList = list0;
        else if (type == 1)
            mList = list1;
        else if (type == 2)
            mList = list2;
    }

    int getPresets() {
        MyApplication grainstorm = MyApplication.getInstance();
        if (grainstorm == null)
            return -1;
        try {
            DocumentFile[] files = MainActivity.getDirContent2(paths[type]);
           if(files == null)
               return -1;
            outerloop:
            for (DocumentFile file : files) {
                for (Presetitem item : mList) {
                    if (Objects.equals(item.documentFile.getUri().getPath(), file.getUri().getPath())) {

                        item.date = DateUtils.getRelativeDateTimeString(

                                grainstorm, // Suppose you are in an activity or other Context subclass

                                item.time * 1000, // The time to display


                                DateUtils.MINUTE_IN_MILLIS, // The resolution. This will display only
                                // minutes (no "3 seconds ago")


                                DateUtils.WEEK_IN_MILLIS, // The maximum resolution at which the time will switch
                                // to default date instead of spans. This will not
                                // display "3 weeks ago" but a full date instead

                                0).toString();
                        continue outerloop;
                    }
                }

                Presetitem h = new Presetitem();
                h.documentFile = file;
                int val;
                if (type == SAVE_PRESET || type == SAVE_PROJECT)
                    val = read_header(file.getUri().toString(), h, presetActivity.type == SAVE_PROJECT);
                else val = read_midiheader(file.getUri().toString(), h);
                if (val == 0) {
                    continue;
                }

                if (h.time == 0)
                    h.time = file.lastModified() / 1000;

                h.date = DateUtils.getRelativeDateTimeString(

                        grainstorm, // Suppose you are in an activity or other Context subclass

                        h.time * 1000, // The time to display


                        DateUtils.MINUTE_IN_MILLIS, // The resolution. This will display only
                        // minutes (no "3 seconds ago")


                        DateUtils.WEEK_IN_MILLIS, // The maximum resolution at which the time will switch
                        // to default date instead of spans. This will not
                        // display "3 weeks ago" but a full date instead

                        0).toString();
                mList.add(h);

            }
            Collections.sort(mList, (o1, o2) -> Long.compare(o2.time, o1.time));
            //thiz.notifyDataSetChanged();
            return mList.size();
        } catch (Exception e) {
            MainActivity.showToast(e.toString());
            return 0;
        }
    }

    @Override
    public int getItemViewType(int position) {

        return position;
    }

    // Create new views (invoked by the layout manager)
    @Override
    @NonNull
    public MyAdapter.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent,
                                                   int viewType) {
        View v = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.presetelement, parent, false);
        return new ViewHolder(v);
    }

    // Replace the contents of a view (invoked by the layout manager)
    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        //SC_HEADER h = mList.get(position);
        holder.name.setText(mList.get(holder.getBindingAdapterPosition()).name);
        holder.date.setText(mList.get(holder.getBindingAdapterPosition()).date);

        holder.deleteButton.setOnClickListener(v -> {
            ViewHolder holder1 = (ViewHolder) v.getTag();
            final int pos = holder1.getBindingAdapterPosition();
            final DocumentFile file = mList.get(pos).documentFile;
            final String name = mList.get(pos).name;

            if (!presetActivity.isFinishing()) {
                presetActivity.runOnUiThread(() -> new AlertDialog.Builder(presetActivity, R.style.MyAlertdialogtheme)
                        .setTitle("DELETE?")
                        .setMessage(name)
                        .setPositiveButton("DELETE",
                                (dialog, which) -> {
                                    try {
                                        if (Objects.requireNonNull(file.getParentFile()).canWrite()) {
                                            if (file.delete()) {
                                                mList.remove(pos);
                                                presetActivity.myAdapter.notifyItemRemoved(pos);
                                                presetActivity.myAdapter.notifyItemRangeChanged(pos, mList.size());
                                                if (mList.isEmpty()) {
                                                    presetActivity.myRecyclerView.setVisibility(View.GONE);
                                                    presetActivity.myEmptyView.setText(emptyString[type]);
                                                    presetActivity.myEmptyView.setVisibility(View.VISIBLE);
                                                }
                                            } else
                                                MainActivity.showToast("Could not delete item.");
                                        } else
                                            MainActivity.showToast("Cannot delete item. Insufficient permissions.");
                                    }
                                    catch (Exception e){
                                        MainActivity.showToast("Could not delete item.");
                                    }

                                }).show());
            }
        });
        if (type == SAVE_PRESET) {
            holder.infoButton.setVisibility(View.VISIBLE);
            holder.infoButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    ViewHolder holder = (ViewHolder) view.getTag();
                    final int pos = holder.getBindingAdapterPosition();
                    final String name = mList.get(pos).name;
                    final long date = mList.get(pos).time * 1000;
                    final String path = mList.get(pos).filepath;
                    final String datestr = new Date(Math.abs(date)).toString();
                    if (!presetActivity.isFinishing()) {
                        final String finalpath;
                        if (path.startsWith("content")) {
                            final Uri uri = Uri.parse(path);
                            if (uri != null) {
                                FileMetaData fileMetaData = getFileMetaData(presetActivity, uri);
                                if (fileMetaData != null)
                                    finalpath = fileMetaData.toString();
                                else finalpath = path;
                            } else finalpath = path;
                        } else if (path.startsWith("rec_mic"))
                            finalpath = "Microphone recording";
                        else if (path.startsWith("presethasaudio"))
                            finalpath = "Included in Preset";
                        else if (path.startsWith("file:")) {
                            finalpath = new File(path).getAbsolutePath();
                        } else
                            finalpath = path;
                        presetActivity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                View root = presetActivity.getLayoutInflater().inflate(R.layout.presetinfo, null);
                                ((TextView) root.findViewById(R.id.created)).setText(datestr);
                                ((TextView) root.findViewById(R.id.filepath)).setText(finalpath);

                                new AlertDialog.Builder(presetActivity, R.style.MyAlertdialogtheme)
                                        .setView(root)
                                        .setTitle(name)
                                        .setNegativeButton("Cancel",
                                                new DialogInterface.OnClickListener() {
                                                    public void onClick(DialogInterface dialog,
                                                                        int which) {
                                                        dialog.dismiss();
                                                    }
                                                }).show();
                            }
                        });
                    }
                }
            });
        } else
            holder.infoButton.setVisibility(View.GONE);

        holder.textLayout.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View view) {
                ViewHolder holder = (ViewHolder) view.getTag();
                final int pos = holder.getAdapterPosition();
                final DocumentFile documentFile = mList.get(pos).documentFile;
                final String presetname = mList.get(pos).name;

                if (documentFile.exists() && documentFile.canRead()) {
                    if (!presetActivity.isFinishing()) {
                        presetActivity.runOnUiThread(() -> new AlertDialog.Builder(presetActivity, R.style.MyAlertdialogtheme)
                                .setTitle("OVERWRITE?")
                                .setMessage(presetname)
                                .setPositiveButton("OVERWRITE",
                                        (dialog, which) -> {
                                            try {
                                                boolean result = documentFile.delete();
                                                if (!result) {
                                                    MainActivity.showToast("Could not delete");
                                                } else
                                                    mList.remove(pos);
                                                presetActivity.finalString = presetname;
                                                presetActivity.finish();
                                            } catch (Exception e) {
                                                MainActivity.showToast("Something went wrong: " + e.toString());
                                            }
                                        })
                                .show());
                    }
                } else MainActivity.showToast("File Error.");
            }
        });
    }

    // Return the size of your dataset (invoked by the layout manager)
    @Override
    public int getItemCount() {
        return mList.size();

    }

    public static class FileMetaData {
        String displayName;
        public long size = 0;
        String mimeType;
        public String path;

        @NotNull
        @Override
        public String toString() {
            return (displayName != null ? displayName : "") + (size != 0 ? "\nSize : " + size / 1024 / 1024 + "MB" : "" + (path != null ? "\nPath : " + path : "" + (mimeType != null ? "\nType : " + mimeType : "")));
        }
    }


    @SuppressLint("Range")
    static FileMetaData getFileMetaData(Context context, Uri uri) {
        try {
            FileMetaData fileMetaData = new FileMetaData();

            if ("file".equalsIgnoreCase(uri.getScheme())) {
                File file;
                if (uri.getPath() != null) {
                    file = new File(uri.getPath());
                    fileMetaData.displayName = file.getName();
                    fileMetaData.size = file.length();
                    fileMetaData.path = file.getPath();
                }
                return fileMetaData;
            } else {
                ContentResolver contentResolver = context.getContentResolver();
                Cursor cursor = contentResolver.query(uri, null, null, null, null);
                fileMetaData.mimeType = contentResolver.getType(uri);


                if (cursor != null && cursor.moveToFirst()) {
                    int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
                    fileMetaData.displayName = cursor.getString(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
                    if (!cursor.isNull(sizeIndex))
                        fileMetaData.size = cursor.getLong(sizeIndex);
                    else
                        fileMetaData.size = -1;

                    try {
                        fileMetaData.path = cursor.getString(cursor.getColumnIndexOrThrow("_data"));
                    } catch (Exception e) {
                        // DO NOTHING, _data does not exist
                    }
                    cursor.close();
                    return fileMetaData;
                }


            }
            return null;
        } catch (Exception e) {
            Log.e("MainActivity", e.toString());
            return null;
        }
    }
}
